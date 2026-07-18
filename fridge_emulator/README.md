# fridge_emulator

Desktop front-end for `fridgemulib` — Dear ImGui + SDL2 + Vulkan.

See `PLAN.md` for the overall design. This file covers what's built today.

## Current state (Phase C: framebuffer display wired)

- SDL2 + Vulkan + Dear ImGui bootstrapped via `imgui_impl_sdl2` /
  `imgui_impl_vulkan` backends.
- `fridgemulib.c` linked as a C++ source (no `posit.c`; PAM16 disabled).
- `FridgeCtx` allocates and lifecycles `FRIDGE_SYSTEM` (CPU, GPU, empty ROM,
  keyboard controller). `FridgeCtx_Reset` reinitialises state.
- `EmuWorker` runs `FRIDGE_sys_tick` on a throttled worker thread (default
  1 MHz) with proper system-timer interval, panic handling, and a mutex for
  main-thread reads.
- `RomLoader_LoadRaw` reads a `.bin` and copies it into `cpu->ram` at
  `FRIDGE_EXECUTABLE_OFFSET` (`0x0200`), then sets `PC = 0x0200`.
- SDL2 keyboard events are mapped to 7-bit ASCII codes and forwarded to
  `FRIDGE_keyboard_press` / `_release` (suppressed while ImGui captures
  keyboard).
- Main menu: `File > Open ROM...`, `Emulation > Run / Pause / Reset / Step`,
  `View > Demo Window / About`.
- A "Registers" panel shows A, B, C, D, E, H, L (editable as hex),
  PC, SP, register pairs, and read-only flag checkboxes. A "Fridge" status
  panel shows CPU state, measured frequency, ROM path, and Run/Pause/Step/
  Reset/Open buttons.
- A "Framebuffer" panel renders the Fridge GPU's visible frame to a 240×180
  `VK_FORMAT_R8G8B8A8_UNORM` device-local Vulkan image via a host-visible
  staging buffer + `vkCmdCopyBufferToImage` per frame, registered as an
  ImGui texture via `ImGui_ImplVulkan_AddTexture`. Displayed through
  `ImGui::Image` with a nearest-sampler draw callback (pixel-perfect, no
  blur) and an adjustable scale slider (1×–8×). Picks text vs. EGA render
  path based on `FRIDGE_gpu_vmode(gpu)`.
- `--smoke-test` CLI mode runs the core headlessly: loads a ROM, steps until
  the CPU halts (or 1 M ticks), prints the final PC/A/state/PANIC and a
  count of non-zero bytes in `gpu.frame_a`. Useful for regression checks
  without a display.

The **Open ROM dialog** uses the host's native file picker: on Linux it
shells out to `zenity` (falling back to `kdialog`), so either must be on
`$PATH`. `File > Open ROM...` (or the status-panel button) opens the picker;
the chosen `.bin` is loaded at `FRIDGE_EXECUTABLE_OFFSET`, the CPU is reset,
and the previously-running worker resumes. For headless automation the
`FRIDGE_OPEN_ROM` env var still auto-loads and starts a ROM at startup
without showing the dialog.

### Phase C validation

A 27-byte test ROM (in Fridge Assembly, under `tests/test_phase_c.x2al`)
switches the GPU to EGA mode, aliases the visible and active frame buffers
via `VPRE` manual_00, then fills the entire visible frame with `0xFF`
(color index 15 = white) through a `VFSA` loop and halts.

Build it with falc (see [Toolchain](#toolchain)):
```bash
cd fridge_emulator && ../build-falc/falc tests/test_phase_c.x2al tests/test_phase_c.bin
../build/fridge_emulator --smoke-test tests/test_phase_c.bin
```
Expected (last 3 lines):
```
[smoke] halted after 172811 steps: PC=0213  A=00  state=1  PANIC=0
[smoke] frame_a non-zero bytes: 21600 / 21600
```
Load the same ROM via `FRIDGE_OPEN_ROM=.../test_phase_c.bin` and the GUI's
"Framebuffer" panel should render a solid 240×180 white image at 3× scale.

### Phase B validation

A 3-byte test ROM (`MVI A, 0x55; HLT`, in `tests/test_phase_b.x2al`):
```bash
cd fridge_emulator && ../build-falc/falc tests/test_phase_b.x2al tests/test_phase_b.bin
../build/fridge_emulator --smoke-test tests/test_phase_b.bin
```
Expected output:
```
[smoke] loaded tests/test_phase_b.bin
[smoke] PC=0200  A=00  state=0  PANIC=0
[smoke] halted after 2 steps: PC=0203  A=55  state=1  PANIC=0
[smoke] frame_a non-zero bytes: 0 / 21600
```

### Phase A leftovers (still present)

- Main menu bar (`File`, `Emulation`, `View`).
- ImGui demo window toggle and an About dialog.

## Files

```
fridge_emulator/
  CMakeLists.txt
  PLAN.md                       overall design
  README.md                     this file
  src/
    main.cpp                    SDL2 + Vulkan + ImGui bootstrap, --smoke-test mode
    app.{h,cpp}                 App struct, per-frame UI, menu, registers/status/framebuffer panels
    fridge_ctx.{h,cpp}          FRIDGE_SYSTEM lifecycle (alloc/reset)
    rom_loader.{h,cpp}          raw .bin -> cpu->ram[0x0200]
    emu_worker.{h,cpp}          throttled worker thread driving FRIDGE_sys_tick
    keymap.{h,cpp}              SDL2 key events -> 7-bit ASCII Fridge key codes
    file_dialog.{h,cpp}         native Open ROM picker (zenity -> kdialog on Linux)
    vulkan_globals.h            extern VkDevice/Queue/etc. for shared use by vk_texture
    vk_texture.{h,cpp}          240x180 RGBA8 device image + staging buffer + per-frame upload
  tests/
    test_phase_b.x2al           MVI A,0x55; HLT — smoke-test ROM #1
    test_phase_b.bin            falc output (regenerable)
    test_phase_c.x2al           EGA mode + VPRE manual_00 + VFSA fill loop + HLT — smoke-test ROM #2
    test_phase_c.bin            falc output (regenerable)
  deps/
    imgui/                      vendored Dear ImGui v1.92.8
    vma/                        vendored Vulkan Memory Allocator v3.4.0 (header-only; not yet used)
```

## Toolchain

Test ROMs are written in Fridge Assembly (`tests/*.x2al`) and assembled with
`falc`. falc now builds on Linux — see the changes done as part of Phase C:
- `falc/main.cpp`: replaced Windows `<tchar.h>`/`_splitpath` with portable
  `std::string` and `find_last_of("/\\")` path splitting; the source-root
  folder is kept with a trailing path separator so falc's
  `sourceRootFolder + sourceFileName` concat works.
- `falc/FridgeAssemblyLanguageCompiler.h`: `STD_PATH` changed from
  `"..\\x2al_std\\"` to `"../x2al_std/"` (forward slashes work on both
  Linux and Win32).
- `falc/FridgeAssemblyLanguageCompiler.cpp`: added `#include <cstring>` for
  `memcpy`; wrapped the three `PAM16C` references in
  `#ifdef FRIDGE_POSIT16_SUPPORT` so the assembler builds whether or not
  PAM16 is enabled in `fridge.h`.

Build falc from the repo root:
```bash
cmake -S falc -B build-falc && cmake --build build-falc -j
```

Then assemble a test ROM from the `fridge_emulator/` directory (so
`STD_PATH "../x2al_std/"` resolves to `<repo>/x2al_std/`):
```bash
cd fridge_emulator
../build-falc/falc tests/test_phase_b.x2al tests/test_phase_b.bin
../build-falc/falc tests/test_phase_c.x2al tests/test_phase_c.bin
```

## Dependencies

System packages (Linux):
```bash
sudo apt install libsdl2-dev vulkan-headers libvulkan-dev   # Debian/Ubuntu
# Fedora: sudo dnf install SDL2-devel vulkan-headers vulkan-loader-devel
```

Vendored (already under `deps/`, no action required):
- `deps/imgui/` — Dear ImGui v1.92.8 (with `backends/`)
- `deps/vma/`   — Vulkan Memory Allocator v3.4.0 (header-only; not yet used by Phase A code)

Optional (recommended for Debug):
```bash
sudo apt install vulkan-validationlayers      # enables per-frame Vulkan debug reports
```
If absent, Debug builds emit a one-line notice and continue without validation.

## Build

```bash
cmake -S fridge_emulator -B fridge_emulator/build
cmake --build fridge_emulator/build -j
./fridge_emulator/build/fridge_emulator
```

For a Release build (no Vulkan debug report):
```bash
cmake -S fridge_emulator -B fridge_emulator/build-release -DCMAKE_BUILD_TYPE=Release
cmake --build fridge_emulator/build-release -j
```

## Smoke test (no display required)

```bash
cd fridge_emulator
../build-falc/falc tests/test_phase_b.x2al tests/test_phase_b.bin
../build/fridge_emulator --smoke-test tests/test_phase_b.bin
```