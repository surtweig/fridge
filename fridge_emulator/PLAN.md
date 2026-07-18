# Plan: `fridge_emulator/` — Dear ImGui + SDL2 + Vulkan desktop front-end for `fridgemulib`

> Status: planning. No code yet.
> Scope: v1 = run + display + keyboard + full debug UI, load raw `.bin` at `0x0200`.
> Out of scope for v1: Posit16/PAM16 support, `FRIDGE_ROM` SD-card streaming protocol,
> in-app falc assembly, `rombuild` integration.

## 1. Goal

A Linux-first desktop app that:

- Loads a raw `.bin` Fridge ROM at `FRIDGE_EXECUTABLE_OFFSET` (0x0200) into
  `cpu->ram`.
- Runs `FRIDGE_sys_tick` on a throttled worker thread at the configured clock
  (default **1 MHz** for debuggability; 10 MHz full-speed exposed as a menu
  option).
- Renders the GPU's visible framebuffer to an ImGui `Image` via a Vulkan
  texture, re-uploaded each frame.
- Shows full debug UI: registers, flags, RAM hex viewer, disassembly
  (with step/run/pause/breakpoint), GPU/sprite/palette state.
- Forwards host SDL2 keyboard events to `FRIDGE_keyboard_press` /
  `FRIDGE_keyboard_release`, and feeds the system timer at
  `target_freq / 256` (mirrors `EmulatorThread::run()`).

Shape mirrors the existing `fridge_opengl_emulator/` design (worker thread +
mutex + `framePresented` signal) but ported to SDL2 / Vulkan / ImGui. Treat
the Qt frontend as the **reference implementation** to lift logic from, not as
a runtime dependency.

## 2. Resolved decisions

- **Stack**: Dear ImGui + SDL2 + Vulkan (raw Vulkan C API — no Vulkan-Hpp, no
  glm/cgmath). We lean on `imgui_impl_sdl2` + `imgui_impl_vulkan` backends,
  which own instance/device/swapchain boilerplate. We only hand-write the
  image-to-texture upload for the Fridge framebuffer.
- **Posit16/PAM16**: disabled. Do not define `FRIDGE_POSIT16_SUPPORT`; do not
  allocate `FRIDGE_PAM16`; do not call `FRIDGE_pam16_*`. Build `posit.c` is
  skipped entirely (conditional in CMake).
- **Disassembler**: a small standalone `disasm.cpp` (~200 lines) that walks the
  `FRIDGE_IRCODE` enum from `include/fridge.h`. **Not** reusing falc's
  two-pass assembler — its parser-assembler shape doesn't fit disassembly.
- **Vulkan Memory Allocator**: included vendored under `deps/`. Cheaper than
  hand-rolling a device-memory sub-allocator and avoids `vkAllocateMemory`
  bookkeeping.
- **Dependencies**: **vendored** into `fridge_emulator/deps/` rather than git
  submodules or `FetchContent`. ImGui + SDL2 + VMA all copied in-tree.
- **Clock default**: 1 MHz at startup (debuggable). The Qt project's full
  `cpuFreqsList` (1 Hz, 2 Hz, 10 Hz, 1 kHz, 1 MHz, 2 MHz, 5 MHz, 10 MHz) is
  exposed via a menu / combo box.
- **GL3 fallback**: keep a `FRIDGE_EMU_BACKEND_GL3` `#define` switch in
  `vk_backend.cpp`'s counterparts so a fallback using
  `imgui_impl_opengl3` + `glTexSubImage2D` is one-day's work away, not a
  rewrite. Not built in v1.

## 3. Directory layout

```
fridge_emulator/
  CMakeLists.txt
  README.md
  PLAN.md                       # this file
  src/
    main.cpp                    # SDL2 + Vulkan bootstrap, ImGui init, main loop
    app.{h,cpp}                 # App struct owning FRIDGE_SYSTEM, ImGui state, worker
    emu_worker.{h,cpp}          # std::thread replacement for EmulatorThread.cpp
    fridge_ctx.{h,cpp}          # owns/lifecycles FRIDGE_SYSTEM, reset, key mapping
    rom_loader.{h,cpp}          # read .bin, memcpy into cpu->ram at 0x0200 (v1)
    disasm.{h,cpp}              # decode next instruction at addr; return text + size
    ram_view.{h,cpp}            # ImGui MemoryEditor wrapper
    ui_panels.{h,cpp}           # registers, flags, GPU, palette, sprites, breakpoints
    vk_backend.{h,cpp}          # Vulkan instance/device/swapchain + ImGui backend init
    vk_texture.{h,cpp}          # 240x180 staging -> device image, sampler, descriptor
  deps/
    imgui/                      # vendored Dear ImGui (with backends/)
    sdl2/                       # vendored SDL2 (or use system package; see CMake)
    vma/                        # vendored Vulkan Memory Allocator (header-only)
```

`fridgemulib.c`, `fridgemulib.h`, and `include/fridge.h` are added as **sources**
in `CMakeLists.txt` (no separate static library) — matches the existing
`fridge_opengl_emulator.pro` pattern and AGENTS.md §8's "fridgemulib as
sources" guidance. `posit.c` / `posit.h` are excluded from the build.

## 4. Conventions to follow (match existing code)

- Include style: `#include <fridge.h>` then `#include "fridgemulib.h"` mirroring
  `fridgemulib/main.cpp` and `fridge_opengl_emulator/mainwindow.cpp`.
- `FRIDGE_SYSTEM` allocated on heap exactly as `MainWindow::initFridge()`
  (lines 45–54), minus PAM16:
  ```cpp
  sys = new FRIDGE_SYSTEM();
  sys->cpu = new FRIDGE_CPU();
  sys->gpu = new FRIDGE_GPU();
  sys->rom = nullptr;        // v1: no ROM controller
  sys->kbrd = new FRIDGE_KEYBOARD_CONTROLLER();
  FRIDGE_cpu_reset(sys->cpu);
  FRIDGE_gpu_reset(sys->gpu);
  ```
- Worker ticker is taken **verbatim** in spirit from `EmulatorThread::run()`
  (lines 35–92): tick-series length, `sysTimerInterval = targetFrequency/256`,
  corePanic handling, measured-frequency computation. Reuse the lock semantics
  with `std::mutex` instead of `QMutex`.
- Visible-frame render path uses both `FRIDGE_gpu_render_ega_rgb8` and
  `FRIDGE_gpu_render_txt_rgb8(..., FRIDGE_gpu_default_glyph_bitmap)` depending
  on `FRIDGE_gpu_vmode(gpu)`, exactly as
  `fridge_opengl_emulator/PixBufferRenderer.cpp:28-30` does.
- Hardcode the default rom-load offset = `FRIDGE_EXECUTABLE_OFFSET` from
  `fridge.h` (do not assume 0x0200 literally — keep the symbol).
- No new comments in source files unless explicitly requested (per AGENTS.md /
  opencode convention). Comments in this PLAN.md and a future README are fine.

## 5. Vulkan specifics for a 240×180 RGB888 blit

`imgui_impl_vulkan` owns the instance/device/swapchain. The only Vulkan code
we write is one extra **image-to-texture** path:

- `VkFormat VK_FORMAT_R8G8B8A8_UNORM`, dimensions from
  `FRIDGE_GPU_FRAME_EGA_WIDTH` × `FRIDGE_GPU_FRAME_EGA_HEIGHT` (240×180 when
  `FRIDGE_VIDEO_240X180` is defined in `fridge.h`, 240×160 otherwise).
- Per frame:
  1. `FRIDGE_gpu_render_ega_rgb8` or `FRIDGE_gpu_render_txt_rgb8` writes
     RGB888 into a host staging buffer.
  2. Inflate to RGBA8 in the same pass (or write RGBA8 directly into a
     slightly larger staging buffer — pick whichever is simpler; recommend a
     single RGBA8 staging buffer textured by a small expand shader).
  3. `vkCmdCopyBufferToImage` to a `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
     image.
  4. Bind the descriptor set once (first frame), then call
     `ImGui::Image((ImTextureID)(intptr_t)ds, size)` inside a window.
- Use **VMA** for device memory. Alternative would be
  `device-local|host-visible` + `vkAllocateMemory` + a hand sub-allocator —
  more code, no upside.
- Validation layers ON in Debug, OFF in Release.
- Render the framebuffer panel at ≥ 3× native size by default (use
  `ImGui::Image` with `uv0`/`uv1` and `ImVec2(240*scale, 180*scale)`).

## 6. Threading model

```
main thread (SDL/ImGui)              worker (std::thread, persistent)
─────────────────────────────        ─────────────────────────────────
loop:                                  while !stop:
  poll SDL events                        if !active: sleep, continue
  -> key events -> enqueue               lock mutex (SetLock)
     FRIDGE_keyboard_press/release       for i in 0..tickSeriesLength:
  ImGui NewFrame                           FRIDGE_sys_tick(sys)
  draw panels:                            if FRIDGE_cpu_flag_PANIC: active=false; signal; break
    - Framebuffer (ImGui::Image)           if (++sysTimerTicksCounter >= sysTimerInterval):
    - Registers/Flags/RAM                   FRIDGE_sys_timer_tick(sys); reset counter
    - Disasm/Breakpoints                    if PANIC: active=false; signal; break
    - GPU/Palette/Sprites                unlock mutex
  Render -> Vulkan present            measuredFreq = 1000 * ticks / elapsed
                                      signal framePresented
```

The `framePresented` signal (cf. `EmulatorThread::framePresented` Qt signal)
wakes the main thread to refresh the debug panels. While the worker holds
the mutex, the main thread may read FRIDGE_SYSTEM only after acquiring the
same mutex (mirror `SetLock` / `ReleaseLock` calls throughout
`mainwindow.cpp`). For the framebuffer specifically: the worker writes GPU
state under lock, the main thread reads it under lock *during* the
framePresented handler and copies pixels to staging — the Vulkan submit
itself can happen outside the lock.

### Breakpoints (v1 minimal)

- Maintain `std::unordered_set<FRIDGE_RAM_ADDR> breakpoints` in the worker,
  guarded by the same mutex.
- After each `FRIDGE_sys_tick`, if `cpu->PC` is in the set: `active = false`,
  emit a `breakpointHit` signal. Resume via the Run button or step.
- v2 can add conditional breakpoints, watch expressions, etc.

## 7. CMake

Per-component CMake (no top-level `CMakeLists.txt` — AGENTS.md §2).

```cmake
cmake_minimum_required(VERSION 3.16)
project(fridge_emulator CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)              # libsdl2-dev (system package OK)
find_package(Vulkan REQUIRED)           # vulkan-headers + loader
find_package(Threads REQUIRED)

# Dear ImGui + backends built from source (vendored under deps/imgui).
set(IMGUI_SOURCES
  deps/imgui/imgui.cpp
  deps/imgui/imgui_draw.cpp
  deps/imgui/imgui_tables.cpp
  deps/imgui/imgui_widgets.cpp
  deps/imgui/backends/imgui_impl_sdl2.cpp
  deps/imgui/backends/imgui_impl_vulkan.cpp
)

add_executable(fridge_emulator
  src/main.cpp
  src/app.cpp
  src/emu_worker.cpp
  src/fridge_ctx.cpp
  src/rom_loader.cpp
  src/disasm.cpp
  src/ui_panels.cpp
  src/ram_view.cpp
  src/vk_backend.cpp
  src/vk_texture.cpp
  ${IMGUI_SOURCES}

  # fridgemulib as sources (matches fridge_opengl_emulator.pro pattern).
  ../fridgemulib/fridgemulib.c
  ../fridgemulib/fridgemulib.h
  ../include/fridge.h
  # posit.c / posit.h intentionally excluded — PAM16 disabled.
)

target_include_directories(fridge_emulator PRIVATE
  src
  ../include
  ../fridgemulib
  deps/imgui
  deps/imgui/backends
  deps/vma/include
)

# Do NOT define FRIDGE_POSIT16_SUPPORT here.

target_link_libraries(fridge_emulator PRIVATE
  SDL2::SDL2
  Vulkan::Vulkan
  Threads::Threads
)
```

`FRIDGE_VIDEO_240X180` is **not** a target option — it lives in
`include/fridge.h` (currently `#define`d) and must stay untouched (AGENTS.md
§7 ABI gotcha).

SDL2 is fine to pull in as a system package (`libsdl2-dev`) on Linux; the
"vendored" decision applies to ImGui and VMA, which are header-style deps
with no distro packaging story we want to depend on.

## 8. Phased implementation

| Phase | Time   | Deliverable |
|-------|--------|-------------|
| A     | ½ day  | Skeleton `fridge_emulator/`, CMake scaffold, cleared ImGui + SDL2 + Vulkan HelloTriangle (port of imgui `example_sdl2_vulkan/main.cpp`). |
| B     | 1 day  | `fridge_ctx` (alloc/reset), `rom_loader` (`.bin` -> `ram[FRIDGE_EXECUTABLE_OFFSET]`), `emu_worker` thread ported from `EmulatorThread::run()`, SDL2 keyboard mapping to `FRIDGE_KEYBOARD_KEY_*` constants. |
| C     | 1 day  | `vk_texture`, hook reupload + `ImGui::Image` into per-frame panel. Default window size ≥ 3× native. |
| D     | 1½–2 days | `ui_panels`: registers/flags (cells editable like `cpuRegAEdit`, read/write PC/SP), `ram_view` (use `ImGui::MemoryEditor` ~4KB vendored widget), `disasm` viewer, breakpoint table, GPU/palette/sprite state panel. |
| E     | 1 day  | Main menu bar (File > Open ROM, Run/Pause/Reset/Step, View menu), config file (recent ROMs, window layout, last clock setting), DPI-aware native window flags, About box. |
| F     | (v2)  | `FRIDGE_ROM` SD-card streaming load path, in-app falc assemble via `FridgeAssemblyLanguageCompiler`, `rombuild` integration, conditional breakpoints/watch expressions, optional shader post-FX (CRT, scanlines). |

## 9. Validation

There is no UI test bench. The green-build signal for v1 is:

1. **App launches**, no Vulkan validation-layer errors in Debug.
2. **Load ROM**: File > Open picks a `.bin`, `FRIDGE_cpu_reset`+`FRIDGE_gpu_reset`
   then `memcpy` into `cpu->ram` at 0x0200. PC visibly set to 0x0200 in the
   registers panel.
3. **Step**: Step button advances PC by one instruction; disasm viewer
   highlights the next instruction; RAM viewer shows correct bytes.
4. **Run/Pause**: Run hits the throttled worker; Pause returns control to
   step mode; the title bar shows the measured frequency (mirror
   `MainWindow`'s `[Running] <freq>` pattern).
5. **Display**: For a known text-mode demo (e.g. assembled `x2al_std/vtext.inc`
   sample), the framebuffer panel renders readable text. For an EGA demo,
   colors look right.
6. **Keyboard**: a Fridge program reading `FRIDGE_keyboard_*` (e.g. a
   `tests/*.falc` keyboard demo) responds to host keys.
7. **Sanitizers**: ASAN/UBSAN clean overnight run on the worker loop with
   `tests/*.falc` loaded.

No `npm run lint` / `ruff` style commands exist in this repo. Treat a clean
run of the existing `fridgemulib` test bench plus the v1 validation above as
the green build signal (AGENTS.md §9).

## 10. Open questions for implementation phase

- SDL2: system package (`apt install libsdl2-dev`) vs vendored? Plan
  assumes system package on Linux; revisit if cross-platform binary
  distribution becomes a goal.
- Config file format: plain INI vs JSON. Recommend INI (matches the
  lightweight spirit of the project; no JSON dep).
- Should the worker thread also drive GPU `FRIDGE_gpu_tick` calls, or only
  `FRIDGE_sys_tick`? Reference Qt code only calls `sys_timer_tick`; need to
  check whether `gpu_tick` is invoked implicitly inside `sys_tick`. (Check
  inside `fridgemulib.c` at implementation time.)
- Keyboard mapping table: the Qt project doesn't expose one explicitly (it
  feeds SDL/Qt key codes through a small mapping). Need to write
  `SDL_Scancode` → `FRIDGE_KEYBOARD_KEY_*_MASK` table at phase B; AGENTS.md
  §7 flags this as part of the boot-loader / emulator ABI, so be careful with
  modifiers.