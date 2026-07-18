#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <cstdint>

#include <SDL.h>

#include "imgui.h"
#include "fridge_ctx.h"
#include "emu_worker.h"
#include "vk_texture.h"

struct App {
    // ImGui options.
    bool show_demo_window = false;
    bool show_about_window = false;
    ImVec4 clear_color = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

    // Owned Fridge state. Worker is bound to fridge.system during App_Init.
    FridgeCtx fridge;
    EmuWorker* worker = nullptr;

    // Vulkan framebuffer texture mirroring FRIDGE_gpu_render_*_rgb8 output.
    VkTexture fb_texture;
    std::vector<uint8_t> fb_rgb;      // RGB888 from FRIDGE_gpu_render_*_rgb8
    std::vector<uint8_t> fb_rgba;     // RGBA8 staging for vk upload (alpha=0xff)
    int fb_scale = 3;                 // display scale (>=1)

    // Path of the currently-loaded ROM (shown in the title bar / status).
    std::string rom_path;
    bool rom_loaded = false;

    // Pending action requested from the UI thread; the main loop peeks
    // these and pumps them while the worker is paused or idle.
    std::atomic<bool> pending_reset {false};

    // Pending open-ROM dialog trigger (set by menu item, consumed by main).
    std::atomic<bool> pending_open_rom {false};

    // Boot-worker preview of last measured frequency (read in App_DrawFrame).
    double measured_freq = 0.0;
};

void App_Init(App& app);
void App_Shutdown(App& app);

// Called once per frame, *outside* the worker lock. Forward host key events
// (already filtered by ImGui's WantCaptureKeyboard) to the Fridge keyboard
// controller. Safe to call from the main thread.
void App_HandleSdlEvent(App& app, const SDL_Event& event);

// Pump pending UI actions that must run on the main thread before drawing
// a frame (e.g. open ROM dialog, reset). Returns true if the worker needs
// to be reactivated after a reset / load.
void App_PumpPending(App& app);

// Draw one ImGui frame's worth of UI. Called between ImGui::NewFrame() and
// ImGui::Render() by main.cpp.
void App_DrawFrame(App& app);