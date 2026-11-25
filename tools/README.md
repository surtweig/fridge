# Fridge PNG converter

`png2fridge.py` converts PNGs to Fridge RLE lines or packed 4-bit pixels. Install
Pillow with `python3 -m pip install Pillow`. The GUI also needs Tkinter and a
graphical session.

Run these commands from the Fridge repository root.

```sh
python3 tools/png2fridge.py
python3 tools/png2fridge.py splash.png splash.rle
python3 tools/png2fridge.py splash.png splash.rle --direction columns --palette colors.pal
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

Both formats contain only payload bytes. Keep the image dimensions, palette,
and slicing direction with the asset. Rows are ordered top to bottom with
pixels left to right. Columns are ordered left to right with pixels top to
bottom.

- **RLE** concatenates one token stream per row or column. The high nibble is
  the color index and the low nibble is the run length minus one. For example,
  `0x4F` encodes 16 pixels of color 4. A run ends at the slice boundary even
  when the next slice starts with the same color. To register a slice with
  `VRS`, set `B` to the slice length in pixels (1–255) and `HL` to its starting
  address in sprite memory. To find the next address, read tokens until the
  sum of their decoded lengths equals the slice length. Use `VRD` with stride
  `0x10` for rows or `0x01` for columns.
- **Raw** packs consecutive pixels in scan order, first pixel in the high
  nibble and second in the low nibble. Packing continues across slice
  boundaries. If the total pixel count is odd, the final low nibble is zero.
  Use row order for Fridge framebuffer data and the hardware sprite layout.
  The [Spartan-6 GPU reference](../fpga/fridge_spartan6/GPU.md#sprite-pixels-and-compositing)
  documents the packed layout and the emulator's sprite-rendering differences.
