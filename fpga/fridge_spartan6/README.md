# Spartan-6 Toolchain (Digilent Atlys / XC6SLX45)

Legacy Xilinx **ISE 14.7** (the last toolchain supporting Spartan-6) running in
Docker on CachyOS, plus host-side programming via Digilent **Adept** (`djtgcfg`).

- Synthesis/P&R/bitstream: ISE 14.7 **WebPACK** in Docker (free; WebPACK covers
  XC6SLX4–XC6SLX75T, so the Atlys' XC6SLX45 is free — but a free WebPACK
  license file is still checked out, see step 4)
- Programming: `djtgcfg` (Digilent Adept) through the Atlys' onboard Digilent
  USB-JTAG (`1443:0007`, FX2-based DJTG protocol — openFPGALoader cannot drive
  this interface)

## Layout

```
Dockerfile              ISE 14.7 WebPACK image (Ubuntu 16.04 base, multi-stage)
docker/                 installer config + entrypoint for the image
ISE/                    official Xilinx ISE 14.7 installer files (you provide)
vendor/                 Digilent Adept debs + user-space extraction
bin/                    host wrappers: ise-run, xst, ngdbuild, map, par, bitgen, trce,
                      fuse/isimgui/vhpcomp (ISim), djtgcfg
constraints/            Digilent's master pin constraints (AtlysGeneral.ucf)
examples/blinky/        smoke-test design for the Atlys
examples/hdmi/          720p60 HDMI rectangle demo with simulation tests
examples/cpu/           Fridge CPU + BRAM smoke test
examples/gpu/           CPU + framebuffer/GPU on the 720p HDMI pipeline
examples/keyboard/      PS/2 keyboard bridge and FIFO demo
examples/rom/           bitstream ROM device and assembly-generated images
examples/text/          hardware-tested 40x20 TEXT mode demo
examples/palette/       programmable TEXT/EGA palette and animation demo
tools/                  Linux falc and boot/ROM image generation
PORTING_PLAN.md         staged Fridge port plan and agreed peripheral choices
setup-host.sh           one-time host setup, part 1 (sudo)
setup-system.sh         one-time host setup, part 2 (sudo)
```

## 1. Installer files

Download from AMD (free account + export-compliance form):
<https://www.amd.com/en/support/downloads/adaptive-socs-and-fpgas/legacy-ise/v2012_4---14_7.html>

Place these 4 files into `ISE/`:

```
Xilinx_ISE_DS_14.7_1015_1-1.tar
Xilinx_ISE_DS_14.7_1015_1-2.zip.xz
Xilinx_ISE_DS_14.7_1015_1-3.zip.xz
Xilinx_ISE_DS_14.7_1015_1-4.zip.xz
```

(Alternative: the single file `Xilinx_ISE_DS_Lin_14.7_1015_1.tar` also works if
you adjust the `Dockerfile` extraction step.)

## 2. One-time host setup

```
sudo ./setup-host.sh
sudo ./setup-system.sh
```

`setup-host.sh`:
- Relocates Docker's `data-root` to `/mnt/data/docker`.
- Installs `openfpgaloader` (useful for FTDI-based external JTAG cables; it
  cannot drive the Atlys' onboard Digilent JTAG).
- Installs a udev rule for the Digilent JTAG (`1443:0007`) so no root is needed.

`setup-system.sh`:
- Relocates **system containerd**'s root to `/mnt/data/containerd`. Docker 29
  uses the system containerd as its image store, so this is where image layers
  actually live (without it, builds fill up `/`).
- Installs Digilent Adept (`djtgcfg`, `dadutil`) system-wide from
  `vendor/adept-root` (extracted from the official Digilent debs).
- Installs `docker-buildx` (optional; enables BuildKit for leaner image builds).

## 3. Build the image

```
docker build -t ise:14.7 .
```

Takes a while (~20–40 min, dominated by the installer). The 8 GB installer
payloads are bind-mounted at build time and do not become image layers.

## 4. Free WebPACK license (needed for map/par/bitgen)

ISE WebPACK 14.7 requires a free, node-locked WebPACK license file (synthesis
(`xst`) runs without it, but `map`/`par`/`bitgen` check it out).

1. Go to <https://www.xilinx.com/getlicense> and sign in with your AMD account.
2. Create a new license: **ISE WebPACK** (free).
3. Host ID type **Ethernet Address (MAC)**, host ID
   `08:00:27:68:c9:35` (the virtual MAC `bin/ise-run` gives the container;
   override with `ISE_LICENSE_MAC` if you prefer another).
4. Download the `.lic` file and save it as `licenses/Xilinx.lic`.

`bin/ise-run` mounts it into the container and sets `XILINXD_LICENSE_FILE`
automatically when the file is present.

## 5. Use the tools

`bin/` wrappers run tools in the container against the current directory:

```
cd examples/blinky
make            # xst -> ngdbuild -> map -> par -> bitgen  -> blinky.bit
make timing     # post-route timing report
make load       # program FPGA SRAM via Digilent Adept (volatile)
```

Any tool can be invoked directly, e.g. `../../bin/xst -help`.

### HDMI Demo

`examples/hdmi/` generates 1280x720 at 60 Hz with a centered 960x640 white
rectangle on blue. See [the demo README](examples/hdmi/README.md) for details:

```
make -C examples/hdmi
make -C examples/hdmi test
make -C examples/hdmi timing
make -C examples/hdmi load
```

The Fridge port roadmap is in [PORTING_PLAN.md](PORTING_PLAN.md).

### Programming Manually

```
djtgcfg enum                          # discover the board (shows "Atlys")
djtgcfg init -d Atlys                 # show JTAG chain (XC6SLX45)
djtgcfg prog -d Atlys -i 0 --file blinky.bit
```

The onboard USB-JTAG speaks Digilent's proprietary FX2/DJTG protocol; only
Digilent Adept (`djtgcfg`) or iMPACT with the Digilent plugin can use it.
`openFPGALoader` works with standard FTDI cables (JTAG-HS2 etc.) attached to an
external JTAG header, but not with this onboard interface. SPI flash
programming is not supported by `djtgcfg` on the Atlys.

## Writing and building your own VHDL

A project needs only four files (copy `examples/blinky/` as a template):

| File | Role |
| --- | --- |
| `top.vhd` | your design (entity name must match `-top` in the `.xst` file) |
| `top.ucf` | pin/timing constraints (net names must match port names) |
| `top.prj` | source list: one `vhdl work "file.vhd"` line per file, **in dependency order** |
| `top.xst` | XST options: part, top module, optimization |
| `Makefile` | drives `xst → ngdbuild → map → par → bitgen` via `../../bin/` |

The classic ISE flow the Makefile automates:

```
xst       synthesize VHDL        -> top.ngc
ngdbuild  merge + apply UCF      -> top.ngd
map       map to FPGA primitives -> top_map.ncd + top.pcf
par       place & route          -> top.ncd
bitgen    generate bitstream     -> top.bit
trce      timing report          -> top.twr   (make timing)
```

Read the reports when things go wrong: `*.syr` (synthesis), `*.mrp` (map),
`*.par` (P&R), `*.twr` (timing). `make clean` removes all generated files.

### XST VHDL notes

- Target is **VHDL-93**. VHDL-2008 constructs are mostly unsupported — avoid
  `all` sensitivity lists in sequential code, generic packages, fixed/float
  packages.
- Prefer `ieee.numeric_std` for arithmetic. Note: mixing `numeric_std` with
  `std_logic_unsigned` in the same file (as the fridge sources do) can produce
  ambiguous-operator errors in XST; if that happens, drop `std_logic_unsigned`
  and cast explicitly with `unsigned()`/`std_logic_vector()`.
- **Clocks**: generate derived clocks from the 100 MHz oscillator with a
  `DCM_SP` or `PLL_BASE` primitive (ISE: "IP Catalog"/CoreGen, or instantiate
  directly). Do **not** use ripple counters as clocks — use clock enables with
  a single clock instead (better timing, no domain issues).
- **Block RAM**: write RAM as a clocked process on a signal array; XST infers
  RAMB16/RAMB8 automatically. For dual-port, use the standard two-process
  pattern (one write port, one read port).
- Constraints syntax is **UCF** (not SDC/XDC). Timing: `TIMESPEC TS_clk =
  PERIOD "clk" 100 MHz HIGH 50%;` with `NET "clk" TNM_NET = clk;`.

### Atlys board reference

Part: `xc6slx45-3-csg324`, 100 MHz oscillator on **L15**. Common pins
(full map in `constraints/AtlysGeneral.ucf`, from Digilent's master UCF):

| Signal | Nets | Notes |
| --- | --- | --- |
| Clock | `clk` (L15) | 100 MHz, single-ended |
| LEDs | `Led<7:0>` | U18, M14, N14, L14, M13, D4, P16, N12 |
| Switches | `sw<7:0>` | `sw<0>` = A10, … |
| Buttons | `btn` (5) | see UCF |
| UART | `UartTx`, `UartRx` | 3.3 V TTL on the "USB UART" port |
| DDR2, HDMI×2, Ethernet, USB, audio, flash | various | banks 2/3 are 2.5 V/1.8 V — set `IOSTANDARD` accordingly |

The example UCF sets only `LOC` for LEDs/clock; Digilent's own demos do the
same (the tools pick a default IOSTANDARD). For real designs, set
`IOSTANDARD = LVCMOS33` on 3.3 V banks explicitly.

### Simulation (ISim)

ISim is included in the image (`bin/fuse`, `bin/isimgui`, `bin/vhpcomp`). CLI
flow from the project directory: `fuse` elaborates the testbench into a
standalone executable, which you then run:

```
../../bin/fuse work.<testbench_entity> -prj <testbench>.prj -o isim.exe
./isim.exe -tclbatch run.tcl        # or: ./isim.exe -gui   (needs X11)
```

A `run.tcl` just drives time: `run all; quit;`. ISim speaks VHDL-93 as well,
so keep testbenches in the same dialect. (GHDL on the host is a faster
alternative for pure-VHDL unit tests: `pacman -S ghdl`.)

### Porting from Quartus / Cyclone V (e.g. the fridge project)

The fridge sources (`fridge/fpga/fridge_graphics_de0cv/`) are mostly
vendor-neutral. What will need attention on Spartan-6:

- `pll.vhd` uses Altera's `altpll` → replace with a Xilinx `PLL_BASE`/`DCM_SP`
  (or a plain counter + clock enable at 100 MHz, which is simpler at these
  speeds).
- Any `altsyncram`/`lpm_*` instantiations → rewrite as inferred RAM (see XST
  notes above).
- UCF instead of QSF; the Atlys has no VGA connector — the video output would
  move to HDMI (the `HDMIOUT*` nets) or a PMOD/VHDCI DAC.
- Check `FridgeRAM.vhd`/boot image: the DE0-CV design may assume SDRAM timing
  or a boot ROM initialization format specific to Altera tools.

## Notes

- **Part numbers**: Atlys = `xc6slx45-3-csg324`. Constraints in
  `examples/blinky/blinky.ucf` come from Digilent's own `AtlysGeneral.ucf`.
- **License**: WebPACK device support is free, but the tools still check out a
  free WebPACK license (see step 4). No paid license is needed for the Atlys.
- **iMPACT** is available (`bin/impact`) but not wired to USB; programming is
  done with `djtgcfg` on the host. If you want iMPACT + the Digilent
  plugin, you'd need to pass `-v /dev/bus/usb:/dev/bus/usb` and install the
  Digilent Adept runtime inside the image.
- **GUI (ISE Project Navigator)** is not set up; the flow is headless/Makefile.
  If wanted: run with `-e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix`.
- Old state remains in `/var/lib/docker` and `/var/lib/containerd` after the
  relocations; delete both later to reclaim disk on `/`.
