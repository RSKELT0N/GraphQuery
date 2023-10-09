#include "frame_menubar.h"

#include "db/system.h"

#include <cstdio>

graphquery::interact::CFrameMenuBar::~CFrameMenuBar() = default;

void graphquery::interact::CFrameMenuBar::Render_Frame() noexcept
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Create")) {
            }
            if (ImGui::MenuItem("Open", "Ctrl+o")) {
                graphquery::database::_log_system->Info("wow");
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
            }
            if (ImGui::MenuItem("Save as..")) {
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

