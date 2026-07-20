#include "ui_symbols.h"
#include "app.h"
#include "emu_worker.h"

#include "imgui.h"
#include <fridge.h>
#include <cstdio>

static bool g_showSystemAliases = false;
static char g_aliasFilterBuf[256] = {};

void DrawAliasesPanel(const DebugInfo& debug)
{
    ImGui::SetNextWindowSize(ImVec2(320, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Aliases"))
    {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Show system aliases", &g_showSystemAliases);
    ImGui::SameLine();
    ImGui::InputTextWithHint("##aliasFilter", "Filter...", g_aliasFilterBuf, sizeof(g_aliasFilterBuf));

    ImGui::Separator();

    ImGui::BeginChild("AliasList", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    for (const auto& kv : debug.aliases)
    {
        const std::string& name = kv.first;
        const std::string& value = kv.second;

        bool isSystem = (name.size() > 0 && name[0] == '#')
                     || (name.find("FRIDGE_") == 0);

        if (!g_showSystemAliases && isSystem)
            continue;

        if (g_aliasFilterBuf[0])
        {
            std::string filter(g_aliasFilterBuf);
            if (name.find(filter) == std::string::npos
                && value.find(filter) == std::string::npos)
                continue;
        }

        ImGui::Text("%s = %s", name.c_str(), value.c_str());
    }

    ImGui::EndChild();
    ImGui::End();
}

void DrawStaticsPanel(const DebugInfo& debug, const FRIDGE_CPU* cpu)
{
    ImGui::SetNextWindowSize(ImVec2(420, 200), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Statics"))
    {
        ImGui::End();
        return;
    }

    if (debug.statics.empty())
    {
        ImGui::TextUnformatted("(no statics)");
        ImGui::End();
        return;
    }

    ImGui::Columns(5, "staticsCols", true);
    ImGui::TextUnformatted("Name");
    ImGui::NextColumn();
    ImGui::TextUnformatted("Addr");
    ImGui::NextColumn();
    ImGui::TextUnformatted("Size");
    ImGui::NextColumn();
    ImGui::TextUnformatted("Compiled");
    ImGui::NextColumn();
    ImGui::TextUnformatted("Runtime");
    ImGui::NextColumn();
    ImGui::Separator();

    for (const auto& s : debug.statics)
    {
        ImGui::TextUnformatted(s.name.c_str());
        ImGui::NextColumn();
        ImGui::Text("0x%04X", s.address);
        ImGui::NextColumn();
        ImGui::Text("%u", (unsigned)s.size);
        ImGui::NextColumn();

        auto formatValue = [](const uint8_t* data, size_t size) {
            if (size > 1 && data[size - 1] == 0)
            {
                bool allPrintable = true;
                for (size_t i = 0; i + 1 < size; ++i)
                    if (data[i] < 0x20 || data[i] > 0x7e)
                        { allPrintable = false; break; }
                if (allPrintable)
                {
                    char buf[128] = {};
                    std::snprintf(buf, sizeof(buf), "\"%s\"", (const char*)data);
                    return std::string(buf);
                }
            }
            if (size == 2)
            {
                FRIDGE_DWORD w = data[0];
                w = (FRIDGE_DWORD)((w << 8) | data[1]);
                char buf[16] = {};
                std::snprintf(buf, sizeof(buf), "0x%04X", (unsigned)w);
                return std::string(buf);
            }
            if (size == 1)
            {
                char buf[16] = {};
                std::snprintf(buf, sizeof(buf), "0x%02X", (unsigned)data[0]);
                return std::string(buf);
            }
            char buf[32] = {};
            std::snprintf(buf, sizeof(buf), "%zu bytes", size);
            return std::string(buf);
        };

        ImGui::TextUnformatted(formatValue(s.data.data(), s.size).c_str());
        ImGui::NextColumn();

        if (cpu && s.address < FRIDGE_RAM_SIZE)
        {
            ImGui::TextUnformatted(formatValue(cpu->ram + s.address, s.size).c_str());
        }
        else
        {
            ImGui::TextUnformatted("--");
        }

        ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::End();
}
