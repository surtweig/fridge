#!/usr/bin/env python3
"""Generate a program's image packages and ISE manifests in its own build dir."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

BOARD = Path(__file__).resolve().parent.parent
RTL = [
    "core/FridgeGlobals.vhd", "core/FridgeIRCodes.vhd",
    "core/FridgeSystemDebug.vhd", "video/FridgeRasterFont.vhd",
    "core/FridgeRAM.vhd", "core/FridgeCPU.vhd",
    "video/video_timing.vhd", "video/fridge_gpu_commands.vhd",
    "video/fridge_sprites.vhd", "video/fridge_gpu.vhd",
    "devices/ps2_receiver.vhd", "devices/fridge_keyboard.vhd",
    "devices/fridge_rom.vhd", "fridge_system.vhd",
    "video/tmds_encoder.vhd", "video/tmds_serializer.vhd",
    "atlys_clocks.vhd", "atlys_top.vhd",
]


def images(program, dest):
    source = BOARD / "programs" / program
    boot = source / "boot.falc"
    sections = sorted((source / "rom").glob("*.hex"))
    if not boot.is_file() or not sections:
        raise SystemExit(f"{source} needs boot.falc and rom/*.hex")
    dest.mkdir(parents=True, exist_ok=True)
    # falc can report compilation errors with exit status 0. Require fresh
    # outputs before replacing previously working packages.
    with tempfile.TemporaryDirectory(dir=dest, prefix=".images-") as temporary:
        stage = Path(temporary)
        subprocess.run([str(BOARD / "tools/falc"), str(boot), str(stage / "boot.bin"),
                        "-vhdl-aggregate"], check=True)
        if not (stage / "boot.bin.vhd").is_file():
            raise SystemExit("falc did not produce an image; see compilation diagnostics")
        (stage / "boot.bin.vhd").rename(stage / "FridgeRAMBootImage.vhd")
        subprocess.run(["python3", str(BOARD / "tools/rom2vhd.py"), "--raw", "-o",
                        str(stage / "FridgeROMImage.vhd"), *map(str, sections)], check=True)
        for name in ("boot.bin", "FridgeRAMBootImage.vhd", "FridgeROMImage.vhd"):
            (stage / name).replace(dest / name)



def manifest(dest, sources, name):
    (dest / f"{name}.prj").write_text("".join(
        f'vhdl work "{path}"\n' for path in sources))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program")
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r"[a-zA-Z0-9_-]+", args.program):
        parser.error("program must be a directory name without slashes")
    dest = args.destination.resolve()
    images(args.program, dest)
    sources = [BOARD / "rtl" / RTL[0], BOARD / "rtl" / RTL[1],
               dest / "FridgeRAMBootImage.vhd", dest / "FridgeROMImage.vhd"]
    sources.extend(BOARD / "rtl" / path for path in RTL[2:])
    manifest(dest, sources, "fridge")
    (dest / "fridge.xst").write_text(
        "run\n-ifn fridge.prj\n-ofn fridge\n-top atlys_top\n"
        "-p xc6slx45-3-csg324\n-opt_mode Speed\n-opt_level 2\n")


if __name__ == "__main__":
    main()
