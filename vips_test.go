package vips

import (
	"bytes"
	"image"
	"image/jpeg"
	"io/ioutil"
	"os"
	"testing"
)

func BenchmarkParallel(b *testing.B) {
	options := Options{Width: 800, Height: 600, Crop: true}
	f, err := os.Open("fixtures/1.jpg")
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
			_, err := Resize(buf, options)
			if err != nil {
				b.Fatal(err)
			}
		}
	})
	b.StopTimer()
}

func BenchmarkSerialized(b *testing.B) {
	options := Options{Width: 800, Height: 600, Crop: true}
	f, err := os.Open("fixtures/1.jpg")
	if err != nil {
		b.Fatal(err)
	}
	buf, err := ioutil.ReadAll(f)
	if err != nil {
		b.Fatal(err)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, err := Resize(buf, options)
		if err != nil {
			b.Fatal(err)
		}
	}
	b.StopTimer()
}

func TestResize(t *testing.T) {
	var testCases = []struct {
		origWidth      int
		origHeight     int
		maxWidth       int
		maxHeight      int
		expectedWidth  uint
		expectedHeight uint
	}{
		{5, 5, 10, 10, 5, 5},
		{10, 10, 5, 5, 5, 5},
		{10, 50, 10, 10, 2, 10},
		{50, 10, 10, 10, 10, 2},
		{50, 100, 60, 90, 45, 90},
		{120, 100, 60, 90, 60, 50},
		{200, 250, 200, 150, 120, 150},
	}

	for index, mt := range testCases {
		img := image.NewGray16(image.Rect(0, 0, mt.origWidth, mt.origHeight))
		buf := new(bytes.Buffer)
		err := jpeg.Encode(buf, img, nil)
		if err != nil {
			t.Errorf(
				"%d. jpeg.Encode(buf, img, nil) error: %#v",
				index, err)
		}

		options := Options{
			Width:        mt.maxWidth,
			Height:       mt.maxHeight,
			Crop:         false,
			Enlarge:      false,
			Extend:       EXTEND_WHITE,
			Interpolator: NOHALO,
			Gravity:      CENTRE,
			Quality:      90,
		}

		newImg, err := Resize(buf.Bytes(), options)
		if err != nil {
			t.Errorf(
				"%d. Resize(imgData, %#v) error: %#v",
				index, options, err)
		}

		outImg, err := jpeg.Decode(bytes.NewReader(newImg))
		if err != nil {
			t.Errorf(
				"%d. jpeg.Decode(newImg) error: %#v",
				index, err)
		}

		newWidth := uint(outImg.Bounds().Dx())
		newHeight := uint(outImg.Bounds().Dy())

		if newWidth != mt.expectedWidth ||
			newHeight != mt.expectedHeight {
			t.Fatalf("%d. Resize(imgData, %#v) => "+
				"width: %v, height: %v, want width: %v, height: %v, "+
				"originl size: %vx%v",
				index, options,
				newWidth, newHeight,
				mt.expectedWidth, mt.expectedHeight,
				mt.origWidth, mt.origHeight,
			)
		}
	}
}

func TestWebpResize(t *testing.T) {
	options := Options{Width: 800, Height: 600, Crop: true}
	img, err := os.Open("fixtures/test.webp")
	if err != nil {
		t.Fatal(err)
	}
	defer img.Close()

	buf, err := ioutil.ReadAll(img)
	if err != nil {
		t.Fatal(err)
	}

	newImg, err := Resize(buf, options)
	if err != nil {
		t.Errorf("Resize(imgData, %#v) error: %#v", options, err)
	}

	if http.DetectContentType(newImg) != WEBP_MIME {
		t.Fatal("Image is not webp valid")
	}
}
