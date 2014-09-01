package vips

import (
	"bytes"
	"io/ioutil"
	"os"
	"testing"
)

func BenchmarkSerialized(b *testing.B) {
	options := Options{Width: 800, Height: 600, Crop: true}
	f, err := os.Open("testdata/1.jpg")
	if err != nil {
		b.Fatal(err)
	}
	buf, err := ioutil.ReadAll(f)
	if err != nil {
		b.Fatal(err)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, err := Resize(bytes.NewReader(buf), options)
		if err != nil {
			b.Fatal(err)
		}
	}
	b.StopTimer()
}

func BenchmarkParallel(b *testing.B) {
	options := Options{Width: 800, Height: 600, Crop: true}
	f, err := os.Open("testdata/1.jpg")
	if err != nil {
		b.Fatal(err)
	}
	buf, err := ioutil.ReadAll(f)
	if err != nil {
		b.Fatal(err)
	}

	b.ResetTimer()
	b.RunParallel(func(pb *testing.PB) {
		for pb.Next() {
			_, err := Resize(bytes.NewReader(buf), options)
			if err != nil {
				b.Fatal(err)
			}
		}
	})
	b.StopTimer()
}
