#!/usr/bin/env python3
"""Run shared-RTL regressions with isolated ISim libraries and image packages."""
import argparse
from pathlib import Path
import subprocess

BOARD = Path(__file__).resolve().parent.parent
RTL = BOARD / "rtl"
COMMON = [RTL / "core/FridgeGlobals.vhd", RTL / "core/FridgeIRCodes.vhd"]
GPU = [RTL / "video/FridgeRasterFont.vhd", RTL / "video/video_timing.vhd",
       RTL / "video/fridge_gpu_commands.vhd", RTL / "video/fridge_sprites.vhd",
       RTL / "video/fridge_gpu.vhd"]
CPU = [RTL / "core/FridgeRAM.vhd", RTL / "core/FridgeCPU.vhd"]
KEYBOARD = [RTL / "devices/ps2_receiver.vhd", RTL / "devices/fridge_keyboard.vhd"]
ROM = [RTL / "devices/fridge_rom.vhd"]
PASS = {
    "cpu": "PASS: CPU/RAM smoke test",
    "gpu": "PASS: GPU framebuffer/scan-out tests",
    "palette": "PASS: programmable palette/CDC tests",
    "graphics": "PASS: framebuffer and sprite board demo",
    "sprites": "PASS: sprite scanout and compositing",
    "access": "PASS: advanced GPU CPU ABI",
    "contract": "PASS: VPAL CPU ABI",
    "ps2_receiver": "PASS: PS/2 receiver tests",
    "keyboard": "PASS: keyboard decoder/FIFO tests",
    "rom": "PASS: ROM device tests",
    "video": "PASS: full 1650x750 raster",
    "serializer": "PASS: OSERDES2/BUFPLL",
    "clocks": "PASS: Atlys clocks and reset",
    "system": "PASS: combined Fridge system",
}


def run(command, dest, log):
    subprocess.run(list(map(str, command)), cwd=dest, stdout=log,
                   stderr=subprocess.STDOUT, check=True)


def test(suite):
    dest = BOARD / ".local/tests" / suite
    dest.mkdir(parents=True, exist_ok=True)
    logpath = dest / "test.log"
    print(f"Running {suite}; log: {logpath}", flush=True)
    with logpath.open("w") as log:
        sources = COMMON.copy()
        if suite in ("system", "graphics"):
            run(["python3", BOARD / "tools/prepare-build.py", ("integration" if suite == "system" else "graphics"), dest], dest, log)
            sources += [dest / "FridgeRAMBootImage.vhd", dest / "FridgeROMImage.vhd",
                        RTL / "core/FridgeSystemDebug.vhd", *GPU, *CPU, *KEYBOARD, *ROM,
                        RTL / "fridge_system.vhd"]
            bench = BOARD / "sim/integration" / f"tb_{suite}.vhd"
        else:
            bench = BOARD / "sim/units" / f"tb_{suite}.vhd"
            if suite == "cpu":
                sources += [BOARD / "examples/cpu/FridgeRAMBootImage.vhd", *CPU]
            elif suite in ("contract", "access"):
                generated = dest / "contract.bin.vhd"
                generated.unlink(missing_ok=True)
                run([BOARD / "tools/falc", (BOARD / "examples/palette/src/vpal_contract.falc" if suite == "contract" else BOARD / "sim/programs/gpu_access.falc"),
                     dest / "contract.bin", "-vhdl-aggregate"], dest, log)
                if not generated.is_file():
                    raise RuntimeError("falc did not generate the VPAL contract image")
                sources += [generated, *GPU, *CPU]
            elif suite in ("gpu", "palette", "sprites"):
                sources += GPU
            elif suite == "keyboard":
                sources += KEYBOARD
            elif suite == "ps2_receiver":
                sources += KEYBOARD[:1]
            elif suite == "rom":
                sources += [BOARD / "examples/rom/FridgeROMImage.vhd", *ROM]
            elif suite == "video":
                sources += [RTL / "video/video_timing.vhd",
                            BOARD / "examples/hdmi/video_pattern.vhd",
                            RTL / "video/tmds_encoder.vhd"]
            elif suite == "serializer":
                sources += [RTL / "video/tmds_serializer.vhd"]
            elif suite == "clocks":
                sources += [RTL / "atlys_clocks.vhd"]
        sources.append(bench)
        (dest / "test.prj").write_text("".join(f'vhdl work "{p}"\n' for p in sources))
        run([BOARD / "bin/fuse", f"work.tb_{suite}", "-prj", "test.prj",
             "-o", "test.exe"], dest, log)
        (dest / "isim.log").unlink(missing_ok=True)
        batch = BOARD / "sim" / ("run_clocks.tcl" if suite == "clocks" else "run.tcl")
        run([BOARD / "bin/ise-run", "./test.exe", "-tclbatch", batch], dest, log)
    result = (dest / "isim.log").read_text()
    if PASS[suite] not in result or "Failure:" in result or "Fatal:" in result:
        raise RuntimeError(f"{suite} did not pass; see {logpath}")
    print(f"PASS: {suite}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("suite", choices=["all", "units", *PASS], default="all", nargs="?")
    args = parser.parse_args()
    compiler = BOARD / ".local/falc-build/falc"
    fridge = BOARD.parent.parent
    host_sources = list((fridge / "falc").glob("*.cpp")) + list((fridge / "falc").glob("*.h")) + list((fridge / "include").glob("*.h"))
    host_sources.append(fridge / "falc/CMakeLists.txt")
    if not compiler.is_file() or any(p.stat().st_mtime_ns > compiler.stat().st_mtime_ns for p in host_sources):
        subprocess.run([str(BOARD / "tools/build-falc.sh")], check=True)
    suites = list(PASS) if args.suite in ("all", "units") else [args.suite]
    if args.suite == "units":
        suites.remove("system")
        suites.remove("graphics")
    for suite in suites:
        test(suite)


if __name__ == "__main__":
    main()
