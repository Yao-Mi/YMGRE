# Synthetic image codec fixtures

These tiny 3×2 fixtures were generated from explicit RGB/gray byte values for this test; they are not third-party art. `rgb.bmp`, PNG variants and the byte arrays in `test_image_codecs.c` share the same samples. `interlaced.png` contains an Adam7 stream, `gamma.png` tags linear gamma without changing samples, and `oversized.png` has a valid large IHDR used to check allocation limits. JPEGs include baseline, progressive, grayscale and unsupported CMYK. `jpeg_rgb_expected.bin` records the standard decoder output for the RGB JPEG.
