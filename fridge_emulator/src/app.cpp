#include "app.h"

#include "imgui.h"

void App_Init(App&) {}
void App_Shutdown(App&) {}

static void DrawMainMenuBar(App& app)
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        ImGui::MenuItem("Open ROM...", nullptr, nullptr, false);
        ImGui::Separator();
        ImGui::MenuItem("Quit", "Alt+F4", nullptr, false);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Emulation"))
    {
        ImGui::MenuItem("Run", nullptr, nullptr, false);
        ImGui::MenuItem("Pause", nullptr, nullptr, false);
        ImGui::MenuItem("Reset", nullptr, nullptr, false);
        ImGui::Separator();
        ImGui::MenuItem("Step", nullptr, nullptr, false);
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

void App_DrawFrame(App& app)
{
    DrawMainMenuBar(app);

    if (app.show_demo_window)
        ImGui::ShowDemoWindow(&app.show_demo_window);

    if (app.show_about_window)
    {
        ImGui::Begin("About Fridge Emulator", &app.show_about_window, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted("Fridge Emulator");
        ImGui::Separator();
        ImGui::TextUnformatted("Phase A skeleton: ImGui + SDL2 + Vulkan.");
        ImGui::Text("ImGui version: %s", IMGUI_VERSION);
        ImGui::End();
    }

    ImGui::Begin("Fridge");
    ImGui::Text("Phase A: skeleton window.");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}