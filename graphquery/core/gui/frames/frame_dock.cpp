#include "frame_dock.hpp"
#include <cstdio>

graphquery::gui::CFrameDock::CFrameDock(GLFWwindow * window, bool is_open) : m_window(window), m_viewp(ImGui::GetMainViewport())
{
    this->m_flags = (ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoBringToFrontOnFocus |
                    ImGuiWindowFlags_MenuBar |
                    ImGuiWindowFlags_NoTitleBar);

    this->m_is_opened = is_open;
}

graphquery::gui::CFrameDock::~CFrameDock() = default;

void graphquery::gui::CFrameDock::Render_Frame() noexcept
{
    m_viewp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(m_viewp->Pos);
    ImGui::SetNextWindowSize(m_viewp->Size);

    ImGui::Begin("Graph Query", &m_is_opened, m_flags);

    ImGui::End();
}

