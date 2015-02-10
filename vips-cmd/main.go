package main

import (
	"flag"
	"fmt"
	"io/ioutil"
	"os"

	"github.com/daddye/vips"
)

func main() {
	filename := ""
	options := vips.Options{Extend: vips.EXTEND_WHITE}
	flag.StringVar(&filename, "file", "", "input file")
	flag.IntVar(&options.Width, "width", 0, "")
	flag.IntVar(&options.Height, "height", 0, "")
	flag.BoolVar(&options.Crop, "crop", false, "")
	flag.BoolVar(&options.Enlarge, "enlarge", false, "")
	flag.IntVar(&options.Quality, "quality", 90, "")
	flag.Parse()

	fmt.Fprintf(os.Stderr, "options: %+v\n", options)

	file, err := os.Open(filename)
	if err != nil {
		flag.PrintDefaults()
		fmt.Fprintln(os.Stderr, err)
		return
	}

	buf, err := ioutil.ReadAll(file)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return
	}

	img, err := vips.Resize(buf, options)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return
	}
	os.Stdout.Write(img)
}
