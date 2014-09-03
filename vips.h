#ifndef GOVIPS_H
#define GOVIPS_H

#include <stdlib.h>
#include <vips/vips.h>

struct options {
    int width;
    int height;
    int quality;
    int crop;
    int rotate;                // default false
    int linear_processing;     // default false
    int interlace;             // default false
    int strip;                 // default true
    VipsExtend extend;         // default to EXTEND_BLACK
    char *interpolator;        // default "bicubic"
    char *convolution_mask;    // default "mild"
};

struct buf {
    void *data;
    int len;
};

typedef struct options Options;
typedef struct buf Buf;

int vips_initialize();

Buf
gvips_resize(Options *o, void *buffer, size_t len);

#endif
