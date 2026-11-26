# Fridge PNG converter

`png2fridge.py` converts PNGs to Fridge RLE streams, importable RLE bitmaps or
packed 4-bit pixels. Install Pillow with `python3 -m pip install Pillow`. The
GUI also needs Tkinter and a graphical session.

Run these commands from the Fridge repository root.

```sh
python3 tools/png2fridge.py
python3 tools/png2fridge.py splash.png splash.rle
python3 tools/png2fridge.py splash.png splash.rle --direction columns --palette colors.pal
python3 tools/png2fridge.py res/fridge-splash-cropped.png splash.rleb --format rle-bitmap
python3 tools/png2fridge.py splash.png splash.bin --format raw
```

In the GUI, open a PNG and choose the output format, slicing direction, and
palette. The preview shows the converted colors and the size of the binary
payload before saving.

The default palette comes from `FRIDGE_gpu_default_palette` in
[`include/fridge.h`](../include/fridge.h). For a custom palette, provide a text
file with 16 whitespace-separated `#RRGGBB` values in GPU index order, starting
with index 0. Load the same colors into Fridge with `VPAL` to reproduce the
preview. The converter composites PNG transparency over palette index 0 and
maps each pixel to the nearest RGB entry without dithering. Ties use the lower
palette index.

## Binary layout

Rows are ordered top to bottom with pixels left to right. Columns are ordered
left to right with pixels top to bottom. Keep the palette and slicing direction
with the asset.

- **RLE** concatenates one token stream per row or column. The high nibble is
  the color index and the low nibble is the run length minus one. For example,
  `0x4F` encodes 16 pixels of color 4. A run ends at the slice boundary even
  when the next slice starts with the same color. Keep the dimensions with the
  asset. To register a slice with `VRS`, set `B` to the slice length in pixels
  (1–255) and `HL` to its starting address in sprite memory. To find the next
  address, read tokens until the sum of their decoded lengths equals the slice
  length. Use `VRD` with stride `0x10` for rows or `0x01` for columns.
- **RLE bitmap** (`rle-bitmap`) starts with two bytes, `LinesCount` followed by
  `LineLength`. For rows these are height and width. For columns these are width
  and height. Both are in the range 1–255. Each line starts with a one-byte
  `SegmentsCount` followed by that many packed RLE tokens.
- **Raw** packs consecutive pixels in scan order, first pixel in the high
  nibble and second in the low nibble. Packing continues across slice
  boundaries. If the total pixel count is odd, the final low nibble is zero.
  Keep the dimensions with the asset.
  Use row order for Fridge framebuffer data and the hardware sprite layout.
  The [Spartan-6 GPU reference](../fpga/fridge_spartan6/GPU.md#sprite-pixels-and-compositing)
  documents the packed layout and the emulator's sprite-rendering differences.

## Import an RLE bitmap in CPM

The RLE bitmap export writes both `splash.rleb` and `splash.cpm` in the output
directory. Keep the files together and regenerate them as a pair when the
image changes. Add `sdk/cpm_std` and `sdk/x2al_std` to the compiler's include paths.

The generated identifiers have an `image_` prefix followed by the original PNG
stem, with punctuation converted to underscores and non-ASCII characters
encoded as code points. For example, `fridge-splash-cropped.png` produces
`image_fridge_splash_cropped_bitmap` even when the binary is named `splash.rleb`.
The companion declares the width, height, line count, line length, draw stride,
and data size. It also imports the binary bytes and initializes the static
`RLELine` array and `RLEBitmap` at link time.

```cpm
include "splash.cpm";

void main()
(
    vega.InitVega vega.VEGA_SWAP_MODE_MANUAL_00;
    vega.DrawRLEBitmap image_fridge_splash_cropped_bitmap, 8, 8,
        image_fridge_splash_cropped_stride, vega.VEGA_RLE_MODE_OPAQUE;
);
```

Each line's `Segments` points directly into the imported array. `DrawRLEBitmap`
draws at native size into the active framebuffer and clips at its right and
bottom edges. It reuses sprite memory at `0x0001`–`0x00FF` and RLE line slot
`255` for each line.

For binaries without a generated companion, `vega.LoadRLEBitmap` can still
validate the data and fill a caller-supplied array of line descriptors at
runtime.
