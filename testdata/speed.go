package main

import (
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"math/rand"
	"os"
	"runtime"
	"time"

	"github.com/daddye/vips"
	"github.com/rcrowley/go-metrics"
)

var timer = metrics.NewTimer()

func main() {

	options := vips.Options{Extend: vips.EXTEND_WHITE}
	flag.IntVar(&options.Width, "width", 800, "")
	flag.IntVar(&options.Height, "height", 600, "")
	flag.BoolVar(&options.Crop, "crop", false, "")
	flag.BoolVar(&options.Enlarge, "enlarge", false, "")
	flag.IntVar(&options.Quality, "quality", 90, "")
	flag.Parse()

	const N = 5
	images := make([][]byte, N)

	for i := 0; i < N; i++ {
		filename := fmt.Sprintf("%d.jpg", i+1)
		fmt.Println("reading:", filename)
		img, err := os.Open(filename)
		if err != nil {
			log.Fatal(err)
		}
		buf, err := ioutil.ReadAll(img)
		if err != nil {
			log.Fatal(err)
		}
		if err := img.Close(); err != nil {
			log.Fatal(err)
		}
		images[i] = buf
	}

	go printStats()

	workers := runtime.GOMAXPROCS(0)
	ch := make(chan []byte, workers)

	for i := 0; i < workers; i++ {
		go func() {
			for buf := range ch {
				timer.Time(func() {
					_, err := vips.Resize(buf, options)
					if err != nil {
						log.Fatal(err)
					}
				})
			}
		}()
	}

	for {
		ch <- images[rand.Intn(N)]
	}
}

func printStats() {
	for range time.Tick(1 * time.Second) {
		fmt.Printf("rate: %.2f - 99%%: %s - total: %d\n",
			timer.RateMean(),
			time.Duration(timer.Percentile(0.99)),
			timer.Count(),
		)
	}
}
