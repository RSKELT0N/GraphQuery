#include "frame_output.h"

graphquery::gui::CFrameLog::CFrameLog()
{
    m_flags = ImGuiWindowFlags_NoCollapse |
              ImGuiWindowFlags_AlwaysVerticalScrollbar;
}

void graphquery::gui::CFrameLog::Render_Frame() noexcept
{
    ImGui::Begin("Output");


    ImGui::End();
}

void graphquery::gui::CFrameLog::Debug(const std::string &) const noexcept {

}

void graphquery::gui::CFrameLog::Info(const std::string &) const noexcept {

}

void graphquery::gui::CFrameLog::Warning(const std::string &) const noexcept {

}

void graphquery::gui::CFrameLog::Error(const std::string &) const noexcept {

}

void graphquery::gui::CFrameLog::Render_Clear_Button() noexcept {

}

