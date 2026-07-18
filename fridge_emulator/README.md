# fridge_emulator

Desktop front-end for `fridgemulib` — Dear ImGui + SDL2 + Vulkan.

See `PLAN.md` for the overall design. This file covers what's built today.

## Current state (Phase A: skeleton)

- SDL2 + Vulkan + Dear ImGui bootstrapped via `imgui_impl_sdl2` /
  `imgui_impl_vulkan` backends.
- Main menu bar (`File`, `Emulation`, `View`) with placeholder items.
- Demo window toggle and a small About dialog.
- No `fridgemulib` linking yet — that arrives in Phase B.

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

## Files

```
fridge_emulator/
  CMakeLists.txt
  PLAN.md                       overall design
  README.md                     this file
  src/
    main.cpp                    SDL2 + Vulkan + ImGui bootstrap (Phase A)
    app.{h,cpp}                 App struct + per-frame UI draw (Phase A: skeleton)
  deps/
    imgui/                      vendored Dear ImGui
    vma/                        vendored Vulkan Memory Allocator
```