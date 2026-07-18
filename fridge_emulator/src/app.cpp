#include "app.h"

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "rom_loader.h"
#include "keymap.h"
#include "file_dialog.h"

#include <cstdio>
#include <cstring>

static bool LoadRomIntoApp(App& app, const char* path, bool was_active)
{
    app.worker->Lock();
    app.worker->SetActive(false);
    FridgeCtx_Reset(app.fridge);
    bool ok = RomLoader_LoadRaw(&app.fridge.cpu, path);
    if (ok)
    {
        app.rom_path = path;
        app.rom_loaded = true;
        if (was_active)
            app.worker->SetActive(true);
    }
    else
    {
        app.rom_path.clear();
        app.rom_loaded = false;
    }
    app.worker->Unlock();
    return ok;
}

void App_Init(App& app)
{
    FridgeCtx_Init(app.fridge);

    app.fb_rgb.resize(size_t(FRIDGE_GPU_FRAME_EGA_WIDTH) * FRIDGE_GPU_FRAME_EGA_HEIGHT * 3);
    app.fb_rgba.resize(size_t(FRIDGE_GPU_FRAME_EGA_WIDTH) * FRIDGE_GPU_FRAME_EGA_HEIGHT * 4);
    VkTexture_Init(app.fb_texture,
                   FRIDGE_GPU_FRAME_EGA_WIDTH, FRIDGE_GPU_FRAME_EGA_HEIGHT,
                   VK_FORMAT_R8G8B8A8_UNORM);

    app.worker = new EmuWorker(&app.fridge.system);
    app.worker->SetTargetFrequency(1000000, 10000);
    app.worker->Start();

    const char* env_rom = std::getenv("FRIDGE_OPEN_ROM");
    if (env_rom && *env_rom)
        LoadRomIntoApp(app, env_rom, true);
}

void App_Shutdown(App& app)
{
    if (app.worker)
    {
        app.worker->Stop();
        delete app.worker;
        app.worker = nullptr;
    }
    VkTexture_Shutdown(app.fb_texture);
    FridgeCtx_Shutdown(app.fridge);
}

void App_HandleSdlEvent(App& app, const SDL_Event& event)
{
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP)
        return;

    int code = KeyMap_SdlToCode(event);
    if (code < 0)
        return;

    bool held = (event.type == SDL_KEYDOWN);
    if (held)
        FRIDGE_keyboard_press(&app.fridge.system, (FRIDGE_WORD)code);
    else
        FRIDGE_keyboard_release(&app.fridge.system, (FRIDGE_WORD)code);
}

void App_PumpPending(App& app)
{
    if (app.pending_open_rom.exchange(false))
    {
        bool was_active = app.worker->IsActive();
        std::string path = FileDialog::OpenFile("Open Fridge ROM");
        if (!path.empty())
        {
            LoadRomIntoApp(app, path.c_str(), was_active);
        }
        else
        {
            std::fprintf(stderr,
                "[app] Open ROM cancelled or no native dialog available "
                "(install zenity or kdialog).\n");
        }
    }

    if (app.pending_reset.exchange(false))
    {
        app.worker->Lock();
        bool was_active = app.worker->IsActive();
        app.worker->SetActive(false);
        FridgeCtx_Reset(app.fridge);
        if (app.rom_loaded && !app.rom_path.empty())
            RomLoader_LoadRaw(&app.fridge.cpu, app.rom_path.c_str());
        if (was_active)
            app.worker->SetActive(true);
        app.worker->Unlock();
    }

    app.measured_freq = app.worker->MeasuredFrequency();
}

static void DrawMainMenuBar(App& app)
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Open ROM...", nullptr))
            app.pending_open_rom.store(true);
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4"))
        {
            SDL_Event quit;
            quit.type = SDL_QUIT;
            SDL_PushEvent(&quit);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Emulation"))
    {
        if (ImGui::MenuItem("Run", nullptr))
            app.worker->SetActive(true);
        if (ImGui::MenuItem("Pause", nullptr))
            app.worker->SetActive(false);
        if (ImGui::MenuItem("Reset", nullptr))
            app.pending_reset.store(true);
        ImGui::Separator();
        if (ImGui::MenuItem("Step", nullptr))
        {
            if (!app.worker->IsActive())
            {
                app.worker->Lock();
                app.worker->StepOnce();
                app.worker->Unlock();
            }
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Demo Window", nullptr, &app.show_demo_window);
        ImGui::MenuItem("About", nullptr, &app.show_about_window);
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

static void DrawRegistersPanel(App& app)
{
    if (!ImGui::Begin("Registers"))
    {
        ImGui::End();
        return;
    }

    FRIDGE_CPU* cpu = &app.fridge.cpu;

    auto HexInput = [](const char* label, FRIDGE_WORD& v) {
        unsigned int tmp = v;
        if (ImGui::InputScalar(label, ImGuiDataType_U32, &tmp, nullptr, nullptr, "%02X",
                               ImGuiInputTextFlags_CharsHexadecimal))
        {
            v = (FRIDGE_WORD)(tmp & 0xff);
        }
    };
    auto HexInput16 = [](const char* label, FRIDGE_RAM_ADDR& v) {
        unsigned int tmp = v;
        if (ImGui::InputScalar(label, ImGuiDataType_U32, &tmp, nullptr, nullptr, "%04X",
                               ImGuiInputTextFlags_CharsHexadecimal))
        {
            v = (FRIDGE_RAM_ADDR)(tmp & 0xffff);
        }
    };

    HexInput ("A", cpu->rA);
    HexInput ("B", cpu->rB);
    HexInput ("C", cpu->rC);
    HexInput ("D", cpu->rD);
    HexInput ("E", cpu->rE);
    HexInput ("H", cpu->rH);
    HexInput ("L", cpu->rL);
    HexInput16("PC", cpu->PC);
    HexInput16("SP", cpu->SP);

    ImGui::Separator();

    ImGui::Text("Pairs: BC=%04X  DE=%04X  HL=%04X",
                FRIDGE_cpu_pair_BC(cpu), FRIDGE_cpu_pair_DE(cpu), FRIDGE_cpu_pair_HL(cpu));

    ImGui::Separator();

    auto FlagCheckbox = [&cpu](const char* name, FRIDGE_WORD (*getter)(const FRIDGE_CPU*)) {
        bool b = getter(cpu) != 0;
        ImGui::BeginDisabled();
        if (ImGui::Checkbox(name, &b)) {}
        ImGui::EndDisabled();
    };
    FlagCheckbox("Sign",   FRIDGE_cpu_flag_SIGN);
    FlagCheckbox("Zero",   FRIDGE_cpu_flag_ZERO);
    FlagCheckbox("Panic",  FRIDGE_cpu_flag_PANIC);
    FlagCheckbox("Aux",    FRIDGE_cpu_flag_AUX);
    FlagCheckbox("Parity", FRIDGE_cpu_flag_PARITY);
    FlagCheckbox("Carry",  FRIDGE_cpu_flag_CARRY);

    ImGui::End();
}

static void DrawFramebufferPanel(App& app)
{
    if (!ImGui::Begin("Framebuffer", nullptr,
                      ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    const uint32_t w = FRIDGE_GPU_FRAME_EGA_WIDTH;
    const uint32_t h = FRIDGE_GPU_FRAME_EGA_HEIGHT;

    if (app.worker)
        app.worker->Lock();

    FRIDGE_GPU* gpu = &app.fridge.gpu;
    FRIDGE_VIDEO_MODE vm = FRIDGE_gpu_vmode(gpu);
    if (vm == FRIDGE_VIDEO_TEXT)
        FRIDGE_gpu_render_txt_rgb8(gpu, app.fb_rgb.data(), FRIDGE_gpu_default_glyph_bitmap);
    else
        FRIDGE_gpu_render_ega_rgb8(gpu, app.fb_rgb.data());

    for (size_t i = 0; i < size_t(w) * h; ++i)
    {
        app.fb_rgba[i * 4 + 0] = app.fb_rgb[i * 3 + 0];
        app.fb_rgba[i * 4 + 1] = app.fb_rgb[i * 3 + 1];
        app.fb_rgba[i * 4 + 2] = app.fb_rgb[i * 3 + 2];
        app.fb_rgba[i * 4 + 3] = 0xff;
    }

    if (app.worker)
        app.worker->Unlock();

    VkTexture_Upload(app.fb_texture, app.fb_rgba.data(), VkDeviceSize(app.fb_rgba.size()));

    ImGui::Text("Mode: %s  (%ux%u)",
                vm == FRIDGE_VIDEO_TEXT ? "Text" : "EGA", w, h);
    ImGui::SliderInt("Scale", &app.fb_scale, 1, 8);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);
    ImGui::Image((ImTextureID)(intptr_t)app.fb_texture.descriptor_set,
                 ImVec2(float(w) * app.fb_scale, float(h) * app.fb_scale));
    dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);

    ImGui::End();
}

static void DrawStatus(App& app)
{
    ImGui::Begin("Fridge");

    const char* state_text = app.worker->IsActive() ? "Running" : "Paused";
    if (FRIDGE_cpu_flag_PANIC(&app.fridge.cpu))
        state_text = "PANIC";
    if (app.fridge.cpu.state == FRIDGE_CPU_HALTED)
        state_text = "Halted";

    ImGui::Text("CPU state: %s", state_text);
    ImGui::Text("Measured freq: %.1f Hz", app.measured_freq);
    ImGui::Text("ROM: %s",
        app.rom_loaded ? app.rom_path.c_str() : "(none)");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::Separator();
    if (app.worker->IsActive())
    {
        if (ImGui::Button("Pause"))
            app.worker->SetActive(false);
    }
    else
    {
        if (ImGui::Button("Run"))
            app.worker->SetActive(true);
        ImGui::SameLine();
        if (ImGui::Button("Step"))
        {
            app.worker->Lock();
            app.worker->StepOnce();
            app.worker->Unlock();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
        app.pending_reset.store(true);
    ImGui::SameLine();
    if (ImGui::Button("Open ROM..."))
        app.pending_open_rom.store(true);

    ImGui::End();
}

void App_DrawFrame(App& app)
{
    DrawMainMenuBar(app);

    DrawStatus(app);
    DrawRegistersPanel(app);
    DrawFramebufferPanel(app);

    if (app.show_demo_window)
        ImGui::ShowDemoWindow(&app.show_demo_window);

    if (app.show_about_window)
    {
        ImGui::Begin("About Fridge Emulator", &app.show_about_window, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted("Fridge Emulator");
        ImGui::Separator();
        ImGui::TextUnformatted("ImGui + SDL2 + Vulkan host front-end for fridgemulib.");
        ImGui::Text("ImGui version: %s", IMGUI_VERSION);
        ImGui::End();
    }
}