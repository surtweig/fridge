# Emulator video test artifacts

Build and run the tests from the repository root:

```sh
cmake -S fridgemulib -B build-fridge-tests
cmake --build build-fridge-tests -j
ctest --test-dir build-fridge-tests --output-on-failure
```

Screens are saved as lossless 24-bit BMP files in `build-fridge-tests/artifacts/` (or
`artifacts/` under the selected CMake build directory). Filenames include the
test family, framebuffer resolution and scene. Running the tests again replaces
the corresponding files. No image libraries are required.

To choose another destination, configure with
`-DFRIDGE_TEST_ARTIFACT_DIR=/absolute/path/to/artifacts`. Generated images are
ignored by Git when saved under an `artifacts/` directory.

RLE tests save three full-frame screens for each of 210x160, 240x160 and 240x180:

- `rle-<resolution>-scaling.bmp`: eight bands, top to bottom, scale the same
  64-pixel source to 16, 32, 48, 64, 96, 128, 160 and 192 output pixels.
- `rle-<resolution>-directions.bmp`: rays in all octants with horizontal,
  vertical, 1:1, 2:1, 3:1, 1:2 and 1:3 slopes.
- `rle-<resolution>-blending.bmp`: four bands, top to bottom, show opaque,
  transparent-zero, checkerboard-dithered and modulo-16 additive drawing over
  the same background.

Each scene is checked against the independent reference rasterizer before it is
saved. A framebuffer comparison failure also saves
`rle-<resolution>-failure-actual.bmp` and
`rle-<resolution>-failure-expected.bmp`, and prints the current drawing registers.
These diagnostic files are only written on failure and may remain from an older
run; use the current test result to determine whether they are relevant.

The existing video tests save `video-<resolution>-ega.bmp`, `-scrolling.bmp`,
`-sprite.bmp` and `-text.bmp` for their two tested resolutions. These capture the
renderer's RGB output; RLE screens render a copy of the active framebuffer so
capturing them does not change the tested GPU state.

When compiling the test executables outside CMake, the default destination is
`artifacts/` relative to the working directory. The `FRIDGE_TEST_ARTIFACT_DIR`
compile definition can override it.
