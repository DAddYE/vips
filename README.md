### Vips for go

This package is powered by the blazingly fast [libvips](https://github.com/jcupitt/libvips) image
processing library, originally created in 1989 at Birkbeck College and currently maintained by 
[JohnCupitt](https://github.com/jcupitt).

This is a loosely port of [sharp](https://github.com/lovell/sharp) an awesome module for node.js
built by [Lovell Fuller](https://github.com/lovell)

The typical use case for this high speed package is to convert large images of many formats
to smaller, web-friendly JPEG, PNG images of varying dimensions.

The performance of JPEG resizing is typically 8x faster than ImageMagick and GraphicsMagick, based
mainly on the number of CPU cores available.

When generating JPEG output all metadata is removed and Huffman tables optimised without having to
use separate command line tools like [jpegoptim](https://github.com/tjko/jpegoptim) and
[jpegtran](http://jpegclub.org/jpegtran/).

## Installation

    go get github.com/daddye/vips

* [libvips](https://github.com/jcupitt/libvips) v7.38.5+

_libvips_ can take advantage of [liborc](http://code.entropywave.com/orc/) if present.

### Install libvips on Mac OS

    brew install homebrew/science/vips --without-fftw --without-libexif --without-libgsf \
      --without-little-cms2 --without-orc --without-pango --without-pygobject3 \
      --without-gobject-introspection --without-python

### Install libvips on Linux

Compiling from source is recommended:

    sudo apt-get install automake build-essential git gobject-introspection \
      libglib2.0-dev libjpeg-turbo8-dev libpng12-dev gtk-doc-tools
    git clone https://github.com/jcupitt/libvips.git
    cd libvips
    ./bootstrap.sh
    ./configure --enable-debug=no --without-python --without-fftw --without-libexif \
      --without-libgf --without-little-cms --without-orc --without-pango --prefix=/usr
    make
    sudo make install
    sudo ldconfig

## Usage

You can use package from the command line (`go install github.com/daddye/vips/vips-cmd`):

    vips-cmd -file test.jpg -width 400 -height 600 > /tmp/test.jpg

Or simply importing the package and then:

```go
options := vips.Options{
	Width:        800,
	Height:       600,
	Crop:         false,
	Extend:       vips.EXTEND_WHITE,
	Interpolator: vips.BILINEAR,
	Gravity:      vips.CENTRE,
	Quality:      95,
}
f, _ := os.Open("/tmp/test.jpg")
inBuf, _ := ioutil.ReadAll(f)
buf, err := vips.Resize(inBuf, options)
if err != nil {
	fmt.Fprintln(os.Stderr, err)
	return
}
// do some with your resized image `buf`
```

## Performance

Test by @lovell

### Test environment

* Intel Xeon [L5520](http://ark.intel.com/products/40201/Intel-Xeon-Processor-L5520-8M-Cache-2_26-GHz-5_86-GTs-Intel-QPI) 2.27GHz 8MB cache
* Ubuntu 13.10
* libvips 7.38.5

### The contenders

* [imagemagick-native](https://github.com/mash/node-imagemagick-native) - Supports Buffers only and blocks main V8 thread whilst processing.
* [imagemagick](https://github.com/rsms/node-imagemagick) - Supports filesystem only and "has been unmaintained for a long time".
* [gm](https://github.com/aheckmann/gm) - Fully featured wrapper around GraphicsMagick.
* [sharp](https://github.com/lovell/sharp) - Caching within libvips disabled to ensure a fair comparison.

### The task

Decompress a 2725x2225 JPEG image, resize and crop to 720x480, then compress to JPEG.

### Results

| Module                | Input  | Output | Ops/sec | Speed-up |
| :-------------------- | :----- | :----- | ------: | -------: |
| imagemagick-native    | buffer | buffer |    0.97 |        1 |
| imagemagick           | file   | file   |    2.49 |      2.6 |
| gm                    | buffer | file   |    3.72 |      3.8 |
| gm                    | buffer | buffer |    3.80 |      3.9 |
| gm                    | file   | file   |    3.67 |      3.8 |
| gm                    | file   | buffer |    3.67 |      3.8 |
| sharp                 | buffer | file   |   13.62 |     14.0 |
| sharp                 | buffer | buffer |   12.43 |     12.8 |
| sharp                 | file   | file   |   13.02 |     13.4 |
| sharp                 | file   | buffer |   11.15 |     11.5 |
| sharp +sharpen        | file   | buffer |   10.26 |     10.6 |
| sharp +progressive    | file   | buffer |    9.44 |      9.7 |
| sharp +sequentialRead | file   | buffer |   11.94 |     12.3 |

You can expect much greater performance with caching enabled (default) and using 16+ core machines.

## Thanks

This module would never have been possible without the help and code contributions of the following people:

* [Lovell Fuller](https://github.com/lovell)
* [John Cupitt](https://github.com/jcupitt)
* [Pierre Inglebert](https://github.com/pierreinglebert)
* [Jonathan Ong](https://github.com/jonathanong)
* [Chanon Sajjamanochai](https://github.com/chanon)
* [Juliano Julio](https://github.com/julianojulio)

## License

Copyright (C) 2014 Davide D'Agostino

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
