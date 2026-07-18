#pragma once

#include "imgui.h"

struct App {
    bool show_demo_window = true;
    bool show_about_window = false;
    ImVec4 clear_color = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
};

void App_Init(App& app);
void App_Shutdown(App& app);
void App_DrawFrame(App& app);