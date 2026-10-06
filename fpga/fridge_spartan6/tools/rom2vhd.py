#!/usr/bin/env python3
"""Pack binary sections into a Fridge ROM image and emit FridgeROMImage.vhd.

This is the ROM counterpart of `falc -vhdl-aggregate`: that one turns a
compiled program into the RAM boot image, this one turns one or more binaries
into the ROM image that the 256-byte-segment ROM device streams to the CPU.

Two layouts are supported:

  --toc (default)  The appliance layout produced by upstream's rombuild:
                   segment 0 is a table of contents holding one 16-bit segment
                   start index per section, and each section follows, zero
                   padded to a whole number of 256-byte segments. This is the
                   form a boot loader consumes.

  --raw            No table of contents; the sections are simply concatenated
                   as raw segments. Use this when the ROM is opaque payload
                   rather than a bootable image.

Inputs are raw binaries, or hex text files (.hex/.txt): whitespace separated
byte values with '#' comments.

Usage:
  rom2vhd.py [-o FridgeROMImage.vhd] [--toc | --raw] SECTION [SECTION ...]
  rom2vhd.py --self-test
"""

import argparse
import os
import re
import sys

SEGMENT_SIZE = 256
MAX_SEGMENTS = 0x10000
# Upstream writes XCM2_ROM_MAX_SECTIONS = TOC_SIZE/2 - 1 entries; the last
# 2 bytes of the TOC segment stay unused.
MAX_SECTIONS = SEGMENT_SIZE // 2 - 1


def parse_hex_text(text, origin):
    """Parse a hex dump: whitespace separated byte values, '#' comments.

    A leading `ADDR:` column (the conventional hex-dump address annotation) is
    ignored, so a fixture can be written as a readable dump with offsets.
    """
    out = bytearray()
    for lineno, line in enumerate(text.splitlines(), 1):
        line = line.split("#", 1)[0]
        line = re.sub(r"^[0-9a-fA-F]{1,4}:\s*", "", line)
        for token in line.split():
            token = token.lower()
            if token.startswith("0x"):
                token = token[2:]
            if not re.fullmatch(r"[0-9a-f]{1,2}", token):
                sys.exit(f"{origin}:{lineno}: not a byte: {token!r}")
            out.append(int(token, 16))
    return bytes(out)


def read_section(path):
    """Return the bytes of one section input (.bin raw or .hex text)."""
    if path == "-":
        return parse_hex_text(sys.stdin.buffer.read().decode("ascii", "replace"), "<stdin>")
    ext = os.path.splitext(path)[1].lower()
    with open(path, "rb") as f:
        raw = f.read()
    if ext in (".hex", ".txt"):
        return parse_hex_text(raw.decode("ascii", "replace"), path)
    return raw


def pad_to_segments(data):
    if not data:
        return bytes(SEGMENT_SIZE)
    rem = len(data) % SEGMENT_SIZE
    return data if rem == 0 else data + bytes(SEGMENT_SIZE - rem)


def pack(sections, toc):
    """Return (image bytes, [(label, byte offset)]) for the given sections.

    The label list describes where each input ended up in the image; the TOC
    segment, when present, is listed first.
    """
    padded = [(name, pad_to_segments(blob)) for name, blob in sections]
    spans = []
    if toc:
        if len(sections) > MAX_SECTIONS:
            sys.exit(f"too many sections: {len(sections)} > {MAX_SECTIONS}")
        toc_bytes = bytearray(SEGMENT_SIZE)
        pos = 1  # segment 0 is the TOC itself
        starts = []
        for name, blob in padded:
            starts.append(pos)
            spans.append((name, pos * SEGMENT_SIZE))
            pos += len(blob) // SEGMENT_SIZE
        total = pos
        if total > MAX_SEGMENTS:
            sys.exit(f"image too large: {total} segments > {MAX_SEGMENTS}")
        # High byte first: BootLoader.x2al reads each entry as D then E and
        # sends D as the segment high byte. Upstream's XCM2ROMImageBuilder
        # writes the entries in host (little-endian) order instead; this tool
        # follows the boot loader, which is the consumer.
        for i in range(MAX_SECTIONS):
            v = starts[i] if i < len(starts) else total
            toc_bytes[2 * i] = (v >> 8) & 0xFF
            toc_bytes[2 * i + 1] = v & 0xFF
        image = bytes(toc_bytes) + b"".join(blob for _, blob in padded)
        return image, [("<TOC>", 0)] + spans

    image = b"".join(blob for _, blob in padded)
    total = len(image) // SEGMENT_SIZE
    if total > MAX_SEGMENTS:
        sys.exit(f"image too large: {total} segments > {MAX_SEGMENTS}")
    offset = 0
    for name, blob in padded:
        spans.append((name, offset))
        offset += len(blob)
    return image, spans


def write_vhdl(image, spans, package, out_path):
    """Write FridgeROMImage.vhd holding the image as a positional hex dump.

    ROM content is data, not code, so unlike the falc boot image (one
    instruction per line, with mnemonics and source comments) it is emitted as
    a compact hex dump: 8 bytes per line, with the address of each 16-byte row
    in a comment. A banner names the section that owns each segment.
    """
    segments = len(image) // SEGMENT_SIZE
    body = []
    body.append("library ieee;")
    body.append("use ieee.std_logic_1164.all;")
    body.append("use ieee.numeric_std.all;")
    body.append("use work.FridgeGlobals.all;")
    body.append("")
    body.append(f"package {package} is")
    body.append("")
    body.append(f"    constant ROM_SEGMENTS : integer := {segments};")
    body.append("    constant ROM_BYTES : integer := ROM_SEGMENTS * 256;")
    body.append("")
    body.append("    type ROM_IMAGE_T is array (0 to ROM_BYTES - 1) of XCM2_WORD;")
    body.append("")
    body.append("    constant ROM_IMAGE : ROM_IMAGE_T :=")
    body.append("    (")
    body.append("        -- Generated by tools/rom2vhd.py. Do not edit;")
    body.append("        -- rebuild the image from its section inputs instead.")
    body.append("        --")

    banners = {offset: label for label, offset in spans}
    rows = []
    for addr in range(0, len(image), 8):
        rows.append(", ".join(f'X"{image[addr + i]:02X}"' for i in range(8)))
    addr = 0
    for n, row in enumerate(rows):
        if addr in banners:
            body.append(f"        -- {banners[addr]} "
                        f"(segment {addr // SEGMENT_SIZE})")
        elif addr % 16 == 0:
            body.append(f"        -- {addr:04X}")
        suffix = "," if n + 1 < len(rows) else ""
        body.append(f"        {row}{suffix}")
        addr += 8

    body.append("    );")
    body.append("")
    body.append(f"end {package};")
    with open(out_path, "w") as f:
        f.write("\n".join(body) + "\n")


def self_test():
    """Round-trip: pack, emit, re-read the VHDL and compare the bytes."""
    import random
    import tempfile

    random.seed(1)
    cases = [
        [("a.bin", bytes(range(256)))],
        [("a.bin", bytes(10)), ("b.bin", bytes([0xFF] * 300)), ("c.bin", b"")],
        [("a.bin", bytes([0x42] * 1024))],
    ]
    for toc in (True, False):
        for sections in cases:
            image, spans = pack(sections, toc=toc)
            if len(image) % SEGMENT_SIZE:
                sys.exit("self-test: image is not a whole number of segments")
            with tempfile.NamedTemporaryFile("w", suffix=".vhd", delete=False) as f:
                path = f.name
            write_vhdl(image, spans, "FridgeROMImage", path)
            text = open(path).read()
            os.unlink(path)
            start = text.index("constant ROM_IMAGE")
            end = text.index(");", start)
            parsed = [int(x, 16) for x in re.findall(r'X"([0-9A-Fa-f]{2})"', text[start:end])]
            if parsed != list(image):
                sys.exit(f"self-test: emitted {len(parsed)} bytes that differ "
                         f"from the {len(image)} packed bytes (toc={toc})")
            for label, offset in spans:
                if offset >= len(image):
                    sys.exit(f"self-test: span {label} outside image")
    print("self-test: OK")
    return 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sections", nargs="*", help="section inputs (.bin or .hex)")
    ap.add_argument("-o", "--output", default="FridgeROMImage.vhd")
    ap.add_argument("--package", default="FridgeROMImage")
    ap.add_argument("--raw", action="store_true",
                    help="no TOC: sections are raw segments, not an appliance image")
    ap.add_argument("--self-test", action="store_true",
                    help="run the packer round-trip test and exit")
    args = ap.parse_args()

    if args.self_test:
        sys.exit(self_test())

    if not args.sections:
        ap.error("no section inputs given")

    sections = [(p, read_section(p)) for p in args.sections]
    image, spans = pack(sections, toc=not args.raw)
    write_vhdl(image, spans, args.package, args.output)
    print(f"{args.output}: {len(image) // SEGMENT_SIZE} segments, "
          f"{len(image)} bytes from {len(sections)} section(s)")


if __name__ == "__main__":
    main()
