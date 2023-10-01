#include "frame_dock.h"

graphquery::interact::CFrameDock::CFrameDock(GLFWwindow * window, bool & is_open) : m_window(window), m_viewp(ImGui::GetMainViewport()), m_is_opened(is_open)
{
    this->m_flags = (ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoBringToFrontOnFocus |
                    ImGuiWindowFlags_MenuBar |
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoNavFocus);

}

graphquery::interact::CFrameDock::~CFrameDock() = default;

void graphquery::interact::CFrameDock::Render_Frame() noexcept
{
    m_viewp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(m_viewp->Pos);
    ImGui::SetNextWindowSize(m_viewp->Size);

    if(ImGui::Begin("Graph Query", &m_is_opened, m_flags)) {}

    ImGui::DockSpace(ImGui::GetID("Graph Query"), ImVec2(0.0F, 0.0F), ImGuiDockNodeFlags_None);

    ImGui::End();
}

