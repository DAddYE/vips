package vips

/*
#cgo pkg-config: vips
#include "vips.h"
*/
import "C"

import (
	"errors"
	"fmt"
	"os"
	"unsafe"
)

const DEBUG = true

var (
	MARKER_JPEG = []byte{0xff, 0xd8}
	MARKER_PNG  = []byte{0x89, 0x50}
)

type ImageType int

const (
	UNKNOWN ImageType = iota
	JPEG
	PNG
)

type Interpolator int

const (
	BICUBIC Interpolator = iota
	BILINEAR
	NOHALO
)

type Extend int

const (
	EXTEND_BLACK Extend = C.VIPS_EXTEND_BLACK
	EXTEND_WHITE Extend = C.VIPS_EXTEND_WHITE
)

var interpolations = map[Interpolator]string{
	BICUBIC:  "bicubic",
	BILINEAR: "bilinear",
	NOHALO:   "nohalo",
}

func (i Interpolator) String() string { return interpolations[i] }

type Options struct {
	Height       int
	Width        int
	Crop         bool
	Rotate       bool
	Enlarge      bool
	Linear       bool
	Interlace    bool
	Strip        bool
	Extend       Extend
	Interpolator Interpolator
	Gravity      Gravity
	Quality      int
}

func init() {
	err := C.vips_initialize()
	if err != 0 {
		C.vips_shutdown()
		panic("unable to start vips!")
	}
	C.vips_concurrency_set(1)
	C.vips_cache_set_max_mem(100 * 1048576) // 100Mb
	C.vips_cache_set_max(500)
}

func Debug() {
	C.im__print_all()
}

func Resize(buf []byte, o Options) ([]byte, error) {
	interpolator := C.CString(o.Interpolator.String())
	co := C.struct_options{
		height: C.int(o.Height),
		width:  C.int(o.Width),
		crop:   btoci(o.Crop),
		rotate: btoci(o.Rotate),
		// enalrge: btoci(o.Enalrge),
		linear_processing: btoci(o.Linear),
		interlace:         btoci(o.Interlace),
		extend:            C.VipsExtend(o.Extend),
		interpolator:      interpolator,
		// gravity: C.Int(o.Gravity),
		quality: C.int(o.Quality),
	}
	cb := C.gvips_resize(&co, unsafe.Pointer(&buf[0]), C.size_t(len(buf)))
	defer C.free(unsafe.Pointer(cb.data))
	defer C.free(unsafe.Pointer(interpolator))
	if int(cb.len) == 0 {
		return nil, resizeError()
	}
	data := C.GoBytes(cb.data, cb.len)
	return data, nil
}

func btoci(v bool) C.int {
	if v {
		return 1
	}
	return 0
}

func resizeError() error {
	s := C.GoString(C.vips_error_buffer())
	C.vips_error_clear()
	C.vips_thread_shutdown()
	return errors.New(s)
}

type Gravity int

const (
	CENTRE Gravity = iota
	NORTH
	EAST
	SOUTH
	WEST
)

func debug(format string, args ...interface{}) {
	if !DEBUG {
		return
	}
	fmt.Fprintf(os.Stderr, format+"\n", args...)
}
