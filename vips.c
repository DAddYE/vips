#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <vips/vips.h>
#include "vips.h"

#define ORIENTATION ("exif-ifd0-Orientation")

static VipsAngle
get_angle(VipsImage *im)
{
    VipsAngle angle;
    const char *orientation;

    angle = VIPS_ANGLE_0;

    if (vips_image_get_typeof(im, ORIENTATION) &&
            !vips_image_get_string(im, ORIENTATION, &orientation))
    {
        if (vips_isprefix("6", orientation))
            angle = VIPS_ANGLE_90;
        else if (vips_isprefix("8", orientation))
            angle = VIPS_ANGLE_270;
        else if (vips_isprefix("3", orientation))
            angle = VIPS_ANGLE_180;

        /*
         * Other values do rotate + mirror, don't bother handling them
         * though, how common can mirroring be.
         *
         * See:
         *
         * http://www.80sidea.com/archives/2316
         */
    }

    return (angle);
}

/*
 * Calculate the shrink factors.
 *
 * We shrink in two stages: first, a shrink with a block average. This can
 * only accurately shrink by integer factors. We then do a second shrink with
 * a supplied interpolator to get the exact size we want.
 *
 * We aim to do the second shrink by roughly half the interpolator's
 * window_size.
 */
static int
calculate_shrink(Options *o, VipsImage *im, double *residual, VipsInterpolate *interp)
{
    VipsAngle angle = get_angle(im);
    gboolean rotate = angle == VIPS_ANGLE_90 || angle == VIPS_ANGLE_270;
    int width = o->rotate && rotate ? im->Ysize : im->Xsize;
    int height = o->rotate && rotate ? im->Xsize : im->Ysize;
    const int window_size = interp ?  vips_interpolate_get_window_size(interp) : 2;

    VipsDirection direction;

    /*
     * Calculate the horizontal and vertical shrink we'd need to fit the
     * image to the bounding box, and pick the biggest.
     *
     * In crop mode we aim to fill the bounding box, so we must use the
     * smaller axis.
     */
    double horizontal = (double) width / o->width;
    double vertical = (double) height / o->height;

    if (o->crop)
    {
        if (horizontal < vertical)
            direction = VIPS_DIRECTION_HORIZONTAL;
        else
            direction = VIPS_DIRECTION_VERTICAL;
    }
    else
    {
        if (horizontal < vertical)
            direction = VIPS_DIRECTION_VERTICAL;
        else
            direction = VIPS_DIRECTION_HORIZONTAL;
    }

    double factor = direction == VIPS_DIRECTION_HORIZONTAL ? horizontal : vertical;

    /*
     * If the shrink factor is <= 1.0, we need to zoom rather than shrink.
     * Just set the factor to 1 in this case.
     */
    double factor2 = factor < 1.0 ? 1.0 : factor;

    /*
     * Int component of factor2.
     *
     * We want to shrink by less for interpolators with larger windows.
     */
    int shrink = VIPS_MAX(1, floor(factor2) / VIPS_MAX(1, window_size / 2));

    if (residual && direction == VIPS_DIRECTION_HORIZONTAL)
    {
        /*
         * Size after int shrink.
         */
        int iwidth = width / shrink;

        /*
         * Therefore residual scale factor is.
         */
        double hresidual = (width / factor) / iwidth;

        *residual = hresidual;
    }
    else if (residual &&
             direction == VIPS_DIRECTION_VERTICAL)
    {
        int iheight = height / shrink;
        double vresidual = (height / factor) / iheight;

        *residual = vresidual;
    }

    return (shrink);
}

/*
 * Find the best jpeg preload shrink.
 */
static int
gvips_find_jpegshrink(Options *o, VipsImage *im)
{
    int shrink = calculate_shrink(o, im, NULL, NULL);

    /*
     * We can't use pre-shrunk images in linear mode. libjpeg shrinks in Y
     * (of YCbCR), not linear space.
     */

    if (o->linear_processing)
        return (1);
    else if (shrink >= 8)
        return (8);
    else if (shrink >= 4)
        return (4);
    else if (shrink >= 2)
        return (2);
    else
        return (1);
}

unsigned char const MARKER_JPEG[] = {0xff, 0xd8};
unsigned char const MARKER_PNG[] = {0x89, 0x50};
unsigned char const MARKER_WEBP[] = {0x52, 0x49};

static VipsInterpolate *
gvips_interpolator(Options *o, VipsObject *process, VipsImage *in)
{
    double residual;
    VipsInterpolate *interp;

    calculate_shrink(o, in, &residual, NULL);

    /*
     * For images smaller than the thumbnail, we upscale with nearest
     * neighbor. Otherwise we make thumbnails that look fuzzy and awful.
     */
    if (!(interp = VIPS_INTERPOLATE(vips_object_new_from_string(
                                        g_type_class_ref(VIPS_TYPE_INTERPOLATE),
                                        residual > 1.0 ? "nearest" : o->interpolator))))
        return (NULL);

    vips_object_local(process, interp);

    return (interp);
}

/*
 * Some interpolators look a little soft, so we have an optional sharpening
 * stage.
 */
static VipsImage *
gvips_sharpen(Options *o, VipsObject *process)
{
    VipsImage *mask;

    if (strcmp(o->convolution_mask, "mild") == 0)
    {
        mask = vips_image_new_matrixv(3, 3,
                                      -1.0, -1.0, -1.0,
                                      -1.0, 32.0, -1.0,
                                      -1.0, -1.0, -1.0);
        vips_image_set_double(mask, "scale", 24);
    }
    else
        vips_warn("gvips", "unknown mask");
    mask = NULL;

    if (mask)
        vips_object_local(process, mask);

    vips_info("gvips", "done with mask %s", o->convolution_mask);
    return (mask);
}

static VipsImage *
gvips_shrink(Options *o, VipsObject *process, VipsImage *in, VipsInterpolate *interp, VipsImage *sharpen)
{
    VipsImage **t = (VipsImage **) vips_object_local_array(process, 10);

    int shrink;
    double residual;
    int tile_width;
    int tile_height;
    int nlines;
    double sigma;

    /*
     * RAD needs special unpacking.
     */
    if (in->Coding == VIPS_CODING_RAD)
    {
        vips_info("gvips", "unpacking Rad to float");

        /* rad is scrgb.
        */
        if (vips_rad2float(in, &t[0], NULL))
            return (NULL);
        in = t[0];
    }

    /*
     * To the processing colourspace. This will unpack LABQ as well.
     */
    vips_info("gvips", "converting to processing space RGB");
    if (vips_colourspace(in, &t[2], VIPS_INTERPRETATION_sRGB, NULL))
        return (NULL);
    in = t[2];

    shrink = calculate_shrink(o, in, &residual, interp);

    vips_info("gvips", "integer shrink by %d", shrink);

    if (vips_shrink(in, &t[3], shrink, shrink, NULL))
        return (NULL);
    in = t[3];

    /*
     * We want to make sure we read the image sequentially.
     * However, the convolution we may be doing later will force us
     * into SMALLTILE or maybe FATSTRIP mode and that will break
     * sequentiality.
     *
     * So ... read into a cache where tiles are scanlines, and make sure
     * we keep enough scanlines to be able to serve a line of tiles.
     *
     * We use a threaded tilecache to avoid a deadlock: suppose thread1,
     * evaluating the top block of the output, is delayed, and thread2,
     * evaluating the second block, gets here first (this can happen on
     * a heavily-loaded system).
     *
     * With an unthreaded tilecache (as we had before), thread2 will get
     * the cache lock and start evaling the second block of the shrink.
     * When it reaches the png reader it will stall until the first block
     * has been used ... but it never will, since thread1 will block on
     * this cache lock.
     */

    vips_get_tile_size(in, &tile_width, &tile_height, &nlines);
    if (vips_tilecache(in, &t[4],
                       "tile_width", in->Xsize,
                       "tile_height", 10,
                       "max_tiles", (nlines * 2) / 10,
                       "access", VIPS_ACCESS_SEQUENTIAL,
                       "threaded", TRUE,
                       NULL))
        return (NULL);
    in = t[4];

    /*
     * If the final affine will be doing a large downsample, we can get
     * nasty aliasing on hard edges. Blur before affine to smooth this out.
     *
     * Don't blur for very small shrinks, blur with radius 1 for x1.5
     * shrinks, blur radius 2 for x2.5 shrinks and above, etc.
     */
    sigma = ((1.0 / residual) - 0.5) / 1.5;
    if (residual < 1.0 && sigma > 0.1)
    {
        if (vips_gaussmat(&t[9], sigma, 0.2,
                          "separable", TRUE,
                          "integer", TRUE,
                          NULL) ||
                vips_convsep(in, &t[5], t[9], NULL))
            return (NULL);
        vips_info("gvips", "anti-alias, sigma %g", sigma);
#ifdef DEBUG
        printf("anti-alias blur matrix is:\n");
        vips_matrixprint(t[9], NULL);
#endif
        in = t[5];
    }

    if (vips_affine(in, &t[6], residual, 0, 0, residual, "interpolate", interp, NULL))
        return (NULL);
    in = t[6];

    vips_info("gvips", "residual scale by %g", residual);
    vips_info("gvips", "%s interpolation", VIPS_OBJECT_GET_CLASS(interp)->nickname);

    /*
     * If we are upsampling, don't sharpen, since nearest looks dumb
     * sharpened.
     */
    if (shrink >= 1 && residual <= 1.0 && sharpen)
    {
        vips_info("gvips", "sharpening thumbnail");
        if (vips_conv(in, &t[8], sharpen, NULL))
            return (NULL);
        in = t[8];
    }

    if (vips_image_get_typeof(in, VIPS_META_ICC_NAME))
    {
        vips_info("gvips", "deleting profile from output image");
        if (!vips_image_remove(in, VIPS_META_ICC_NAME))
            return (NULL);
    }

    return (in);
}

/*
 * Crop down to the final size, if crop_image is set.
 */
static VipsImage *
gvips_crop(Options *o, VipsObject *process, VipsImage *im)
{
    // avoid operations if not needed
    if (im->Xsize == o->width && im->Ysize == o->height)
        return (im);

    VipsImage **t = (VipsImage **) vips_object_local_array(process, 2);

    if (o->crop)
    {
        int left = (im->Xsize - o->width) / 2;
        int top = (im->Ysize - o->height) / 2;

        if (vips_embed(im, &t[0], left, top, o->width, o->height, "extend", o->extend, NULL))
            return (NULL);
        im = t[0];
    }

    return (im);
}

/*
 * Auto-rotate, if rotate_image is set.
 */
static VipsImage *
gvips_rotate(Options *o, VipsObject *process, VipsImage *im)
{
    VipsImage **t = (VipsImage **) vips_object_local_array(process, 2);
    VipsAngle angle = get_angle(im);

    if (o->rotate && angle != VIPS_ANGLE_0)
    {
        /*
         * Need to copy to memory, we have to stay seq.
         */
        t[0] = vips_image_new_memory();
        if (vips_image_write(im, t[0]) ||
                vips_rot(t[0], &t[1], angle, NULL))
            return (NULL);
        im = t[1];

        (void) vips_image_remove(im, ORIENTATION);
    }

    return (im);
}

Buf
gvips_resize(Options *o, void *buffer, size_t len)
{
    /*
     * Set some defaults
     */
    if (o->convolution_mask == NULL)
        o->convolution_mask = "mild";

    if (o->interpolator == NULL)
        o->interpolator = "bicubic";

    if (o->quality == 0)
        o->quality = 95;

    VipsImage *im;
    Buf buf;

    vips_info("gvips", "checking buffer marker");

    if (memcmp(MARKER_JPEG, buffer, 2) == 0)
    {
        if (vips_jpegload_buffer(buffer, len, &im, "access", VIPS_ACCESS_SEQUENTIAL, NULL))
            return (buf);
    }
    else if (memcmp(MARKER_PNG, buffer, 2) == 0)
    {
        if (vips_pngload_buffer(buffer, len, &im, "access", VIPS_ACCESS_SEQUENTIAL, NULL))
            return (buf);
    }
    else if (memcmp(MARKER_WEBP, buffer, 2) == 0)
    {
        if (vips_webpload_buffer(buffer, len, &im, "access", VIPS_ACCESS_SEQUENTIAL, NULL))
            return (buf);
    }


    /*
     * Hang resources for processing this thumbnail off @process.
     */
    VipsObject *process = VIPS_OBJECT(vips_image_new());
    vips_object_local(process, im);


    /*
     * Process the image
     */
    VipsImage *sharpen = gvips_sharpen(o, process);

    VipsInterpolate *interp;
    VipsImage *shrinked;
    VipsImage *cropped;
    VipsImage *rotated;

    if (!(interp = gvips_interpolator(o, process, im)) ||
            !(shrinked = gvips_shrink(o, process, im, interp, sharpen)) ||
            !(cropped = gvips_crop(o, process, shrinked)) ||
            !(rotated = gvips_rotate(o, process, cropped)))
        return (buf);

    vips_info("gvips", "saving the output");

    /*
     * Save the output
     */
    int e = vips_jpegsave_buffer(rotated, &buffer, &len,
                                 "strip", o->strip,
                                 "Q", o->quality,
                                 "optimize_coding", TRUE,
                                 "interlace", o->interlace,
                                 NULL);
    g_object_unref(process);

    if (e == -1)
        return (buf);

    buf.data = buffer;
    buf.len = len;

    vips_thread_shutdown();

    return buf;
}

int
vips_initialize()
{
    return VIPS_INIT("govips");
};
