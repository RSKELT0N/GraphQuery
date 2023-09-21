#include "frame_output.hpp"

graphquery::gui::CFrameLog::CFrameLog()
{
    m_flags = ImGuiWindowFlags_NoCollapse |
              ImGuiWindowFlags_AlwaysVerticalScrollbar;
}

graphquery::gui::CFrameLog::~CFrameLog() = default;

void graphquery::gui::CFrameLog::Render_Frame() noexcept
{
    ImGui::Begin("Output");


    ImGui::End();
}

