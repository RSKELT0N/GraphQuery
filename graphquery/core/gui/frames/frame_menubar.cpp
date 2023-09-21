#include "frame_menubar.hpp"
#include <cstdio>

graphquery::gui::CFrameMenuBar::~CFrameMenuBar() = default;

void graphquery::gui::CFrameMenuBar::Render_Frame() noexcept
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Create")) {
            }
            if (ImGui::MenuItem("Open", "Ctrl+o")) {
                printf("wow\n");
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

