#!/usr/bin/env python3
"""Convert PNG images to Fridge RLE lines or packed 4-bit pixels."""

import argparse
from functools import lru_cache
from itertools import groupby
from pathlib import Path
import re
import sys
from tempfile import NamedTemporaryFile

try:
    from PIL import Image
except ImportError as error:
    raise SystemExit("png2fridge requires Pillow (python3 -m pip install Pillow).") from error

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
    from PIL import ImageTk
except ImportError:
    tk = None


FRIDGE_HEADER = Path(__file__).resolve().parents[1] / "include" / "fridge.h"
PALETTE_SIZE = 16
MAX_RUN_LENGTH = 16
MAX_LINE_LENGTH = 255  # VRS takes the nominal length in register B.
FORMATS = ("rle", "raw")
DIRECTIONS = ("rows", "columns")
EXTENSIONS = {"rle": ".rle", "raw": ".bin"}


def load_palette(path=None):
    """Return the ordered RGB triples as 48 bytes."""
    if path is None:
        header = FRIDGE_HEADER.read_text(encoding="utf-8")
        header = re.sub(r"/\*.*?\*/|//[^\n]*", "", header, flags=re.DOTALL)
        initializer = re.search(
            r"\bFRIDGE_gpu_default_palette\s*\[[^\]]+\]\s*=\s*\{([^}]+)\}", header
        )
        if initializer is None:
            raise ValueError(f"Cannot find FRIDGE_gpu_default_palette in {FRIDGE_HEADER}.")
        entries = initializer[1].strip().rstrip(",").split(",")
        rgb = bytes(int(entry, 0) for entry in entries)
        if len(rgb) != PALETTE_SIZE * 3:
            raise ValueError(f"Expected 16 RGB colors in {FRIDGE_HEADER}.")
        return rgb

    colors = path.read_text(encoding="utf-8-sig").split()
    if len(colors) != PALETTE_SIZE or any(
        re.fullmatch(r"#[0-9a-fA-F]{6}", color) is None for color in colors
    ):
        raise ValueError(f"{path.name} must contain exactly 16 colors as #RRGGBB values.")
    return bytes.fromhex("".join(color[1:] for color in colors))


def load_png(path):
    with Image.open(path, formats=["PNG"]) as image:
        return image.convert("RGBA")


def quantize(image, palette):
    """Map pixels to the nearest palette entry in RGB space, without dithering."""
    colors = list(zip(palette[::3], palette[1::3], palette[2::3]))
    background = Image.new("RGBA", image.size, (*colors[0], 255))
    rgb = Image.alpha_composite(background, image).convert("RGB").tobytes()

    @lru_cache(maxsize=4096)
    def nearest(red, green, blue):
        return min(
            range(PALETTE_SIZE),
            key=lambda index: (red - colors[index][0]) ** 2
            + (green - colors[index][1]) ** 2
            + (blue - colors[index][2]) ** 2,
        )

    indices = bytes(nearest(*pixel) for pixel in zip(rgb[::3], rgb[1::3], rgb[2::3]))
    indexed = Image.frombytes("P", image.size, indices)
    indexed.putpalette(palette)
    return indexed


def encode(indexed, output_format, direction):
    line_length = indexed.width if direction == "rows" else indexed.height
    if output_format == "rle" and line_length > MAX_LINE_LENGTH:
        axis = "row" if direction == "rows" else "column"
        raise ValueError(f"Each RLE {axis} must fit in {MAX_LINE_LENGTH} pixels (got {line_length}).")

    scan = indexed if direction == "rows" else indexed.transpose(Image.Transpose.TRANSPOSE)
    pixels = scan.tobytes()
    if output_format == "raw":
        packed = bytearray((left << 4) | right for left, right in zip(pixels[::2], pixels[1::2]))
        if len(pixels) % 2:
            packed.append(pixels[-1] << 4)
        return bytes(packed)

    encoded = bytearray()
    for start in range(0, len(pixels), line_length):
        for color, run in groupby(pixels[start:start + line_length]):
            length = sum(1 for _ in run)
            for offset in range(0, length, MAX_RUN_LENGTH):
                encoded.append((color << 4) | (min(length - offset, MAX_RUN_LENGTH) - 1))
    return bytes(encoded)


def write_output(path, data, source, palette_path):
    if path.exists() and any(
        path.samefile(protected)
        for protected in (source, palette_path)
        if protected is not None and protected.exists()
    ):
        raise ValueError("Choose an output path separate from the input PNG and palette.")

    output = NamedTemporaryFile(dir=path.parent, delete=False)
    temporary_path = Path(output.name)
    try:
        with output:
            output.write(data)
        temporary_path.replace(path)
    finally:
        temporary_path.unlink(missing_ok=True)


class ConverterWindow:
    def __init__(self, root):
        self.root = root
        self.source = None
        self.image = None
        self.indexed = None
        self.preview = None
        self.preview_photo = None
        self.data = None
        self.default_palette = load_palette()
        self.palette = self.default_palette
        self.palette_path = None

        root.title("Fridge PNG Converter")
        root.geometry("720x560")
        root.minsize(475, 330)
        panel = ttk.Frame(root, padding=12)
        panel.pack(fill="both", expand=True)

        files = ttk.Frame(panel)
        files.pack(fill="x")
        ttk.Button(files, text="Open PNG…", command=self.open_png).pack(side="left")
        self.source_name = tk.StringVar(value="Choose a PNG to convert")
        ttk.Label(files, textvariable=self.source_name, width=1).pack(
            side="left", fill="x", expand=True, padx=8
        )
        self.save_button = ttk.Button(files, text="Save as…", command=self.save, state="disabled")
        self.save_button.pack(side="right")

        options = ttk.Frame(panel)
        options.pack(fill="x", pady=(10, 0))
        self.output_format = tk.StringVar(value="rle")
        self.direction = tk.StringVar(value="rows")
        for label, variable, values in (
            ("Format", self.output_format, FORMATS),
            ("Slicing", self.direction, DIRECTIONS),
        ):
            ttk.Label(options, text=label).pack(side="left", padx=(0, 6))
            menu = ttk.Combobox(
                options, textvariable=variable, values=values, width=10, state="readonly"
            )
            menu.pack(side="left", padx=(0, 16))
            menu.bind("<<ComboboxSelected>>", self.update_encoding)

        palette_tools = ttk.Frame(panel)
        palette_tools.pack(fill="x", pady=(10, 0))
        ttk.Button(palette_tools, text="Open palette…", command=self.open_palette).pack(side="left")
        ttk.Button(palette_tools, text="Use default", command=self.use_default_palette).pack(
            side="left", padx=6
        )
        self.palette_name = tk.StringVar()
        ttk.Label(palette_tools, textvariable=self.palette_name, width=1).pack(
            side="left", fill="x", expand=True
        )
        self.swatches = tk.Canvas(panel, width=PALETTE_SIZE * 22, height=20, highlightthickness=0)
        self.swatches.pack(anchor="w", pady=(6, 10))

        self.canvas = tk.Canvas(panel, background="#30343a", highlightthickness=0, width=1, height=1)
        self.canvas.pack(fill="both", expand=True)
        self.canvas_image = self.canvas.create_image(0, 0)
        self.canvas.bind("<Configure>", self.draw_preview)

        self.summary = tk.StringVar()
        ttk.Label(panel, textvariable=self.summary, wraplength=450).pack(fill="x", pady=(8, 0))
        self.update_palette()

    def open_png(self):
        filename = filedialog.askopenfilename(
            parent=self.root, filetypes=[("PNG images", "*.png *.PNG"), ("All files", "*")]
        )
        if not filename:
            return
        path = Path(filename)
        try:
            image = load_png(path)
        except (OSError, ValueError, Image.DecompressionBombError) as error:
            messagebox.showerror("Cannot open PNG", str(error), parent=self.root)
            return
        self.source = path
        self.image = image
        self.source_name.set(path.name)
        self.update_preview()

    def open_palette(self):
        filename = filedialog.askopenfilename(
            parent=self.root, filetypes=[("Palette files", "*.pal *.txt"), ("All files", "*")]
        )
        if not filename:
            return
        path = Path(filename)
        try:
            palette = load_palette(path)
        except (OSError, ValueError) as error:
            messagebox.showerror("Cannot open palette", str(error), parent=self.root)
            return
        self.palette = palette
        self.palette_path = path
        self.update_palette()

    def use_default_palette(self):
        self.palette = self.default_palette
        self.palette_path = None
        self.update_palette()

    def update_palette(self):
        self.palette_name.set(self.palette_path.name if self.palette_path else "Fridge default palette")
        self.swatches.delete("all")
        for index in range(PALETTE_SIZE):
            start = index * 3
            color = "#" + self.palette[start:start + 3].hex()
            self.swatches.create_rectangle(
                index * 22, 0, (index + 1) * 22, 20, fill=color, outline="#808080"
            )
        self.update_preview()

    def update_preview(self):
        if self.image is None:
            return
        self.indexed = quantize(self.image, self.palette)
        self.preview = self.indexed.convert("RGB")
        self.draw_preview()
        self.update_encoding()

    def draw_preview(self, _event=None):
        if self.preview is None:
            return
        width, height = self.canvas.winfo_width(), self.canvas.winfo_height()
        scale = min(width / self.preview.width, height / self.preview.height)
        size = (max(1, int(self.preview.width * scale)), max(1, int(self.preview.height * scale)))
        preview = self.preview.resize(size, Image.Resampling.NEAREST)
        self.preview_photo = ImageTk.PhotoImage(preview, master=self.root)
        self.canvas.itemconfigure(self.canvas_image, image=self.preview_photo)
        self.canvas.coords(self.canvas_image, width // 2, height // 2)

    def update_encoding(self, _event=None):
        if self.indexed is None:
            return
        direction = self.direction.get()
        try:
            self.data = encode(self.indexed, self.output_format.get(), direction)
        except ValueError as error:
            self.data = None
            self.save_button.configure(state="disabled")
            self.summary.set(str(error))
            return
        width, height = self.indexed.size
        line_count, line_length = (height, width) if direction == "rows" else (width, height)
        self.summary.set(
            f"{width} × {height} pixels   •   {line_count} {direction} × {line_length} pixels   •   "
            f"{len(self.data):,} bytes"
        )
        self.save_button.configure(state="normal")

    def save(self):
        if self.data is None:
            return
        extension = EXTENSIONS[self.output_format.get()]
        filename = filedialog.asksaveasfilename(
            parent=self.root,
            initialdir=self.source.parent,
            initialfile=self.source.stem + extension,
            defaultextension=extension,
            filetypes=[("Fridge data", f"*{extension}"), ("All files", "*")],
        )
        if filename:
            try:
                write_output(Path(filename), self.data, self.source, self.palette_path)
            except (OSError, ValueError) as error:
                messagebox.showerror("Cannot save output", str(error), parent=self.root)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, epilog="Run without arguments to open the GUI.")
    parser.add_argument("input", type=Path, nargs="?", help="input PNG")
    parser.add_argument("output", type=Path, nargs="?", help="output binary file")
    parser.add_argument("--format", choices=FORMATS, default="rle", help="output format (default: rle)")
    parser.add_argument("--direction", choices=DIRECTIONS, default="rows", help="scan order (default: rows)")
    parser.add_argument("--palette", type=Path, help="text file with 16 #RRGGBB colors in index order")
    argv = sys.argv[1:] if argv is None else argv
    args = parser.parse_args(argv)

    if not argv:
        if tk is None:
            parser.exit(1, "The GUI requires Tkinter and Pillow's ImageTk support.\n")
        try:
            root = tk.Tk()
            ConverterWindow(root)
        except (tk.TclError, OSError, ValueError) as error:
            parser.exit(1, f"Cannot open converter GUI: {error}\n")
        root.mainloop()
        return

    if args.input is None or args.output is None:
        parser.error("command-line conversion requires both input and output paths")
    try:
        palette = load_palette(args.palette)
        indexed = quantize(load_png(args.input), palette)
        data = encode(indexed, args.format, args.direction)
        write_output(args.output, data, args.input, args.palette)
    except (OSError, ValueError, Image.DecompressionBombError) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")


if __name__ == "__main__":
    main()
