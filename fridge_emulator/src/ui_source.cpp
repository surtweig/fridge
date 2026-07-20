#include "ui_source.h"
#include "app.h"
#include "emu_worker.h"

#include "imgui.h"
#include <fridge.h>
#include <cstdio>

void InitSourceBreakpoints()
{
}

void DrawSourcePanel(App& app, const DebugInfo& debug, const FRIDGE_CPU* cpu)
{
    ImGui::SetNextWindowSize(ImVec2(520, 380), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Source"))
    {
        ImGui::End();
        return;
    }

    FRIDGE_RAM_ADDR pc = cpu->PC;
    size_t activeLineIdx = debug.FindLineIndexForPC(pc);

    ImGui::Text("Source (%zu lines)", debug.sourceLines.size());
    ImGui::SameLine();
    ImGui::TextDisabled("PC=0x%04X", pc);
    ImGui::Separator();

    ImGui::BeginChild("SourceScroller", ImVec2(0, 0), true);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rowHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2;

    for (size_t i = 0; i < debug.sourceLines.size(); ++i)
    {
        const SourceLine& sl = debug.sourceLines[i];
        bool isActive = (i == activeLineIdx);
        bool hasBP = (sl.address > 0 && app.breakpoints.count(sl.address) > 0);

        ImVec2 cursor = ImGui::GetCursorScreenPos();

        if (isActive)
        {
            dl->AddRectFilled(cursor,
                              ImVec2(cursor.x + ImGui::GetContentRegionAvail().x,
                                     cursor.y + rowHeight),
                              IM_COL32(40, 120, 40, 180));
        }
        else if (hasBP)
        {
            dl->AddRectFilled(cursor,
                              ImVec2(cursor.x + ImGui::GetContentRegionAvail().x,
                                     cursor.y + rowHeight),
                              IM_COL32(120, 30, 30, 180));
        }

        char label[128];
        std::snprintf(label, sizeof(label), "%5d  %s##src%d",
                      sl.lineNumber,
                      sl.rawText.c_str(),
                      (int)i);

        bool pushedColor = false;
        if (isActive)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 255, 100, 255));
            pushedColor = true;
        }

        ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowOverlap;
        bool clicked = ImGui::Selectable(label, false, flags);

        if (pushedColor)
            ImGui::PopStyleColor();

        if (clicked && sl.address > 0)
        {
            if (app.breakpoints.count(sl.address))
                app.breakpoints.erase(sl.address);
            else
                app.breakpoints.insert(sl.address);
        }

        if (isActive)
            ImGui::SetScrollHereY(0.25f);
    }

    ImGui::EndChild();
    ImGui::End();
}
