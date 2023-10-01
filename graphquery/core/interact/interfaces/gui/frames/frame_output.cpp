#include "frame_output.h"

graphquery::interact::CFrameLog::CFrameLog()
{
    m_flags = ImGuiWindowFlags_NoCollapse |
              ImGuiWindowFlags_AlwaysVerticalScrollbar;
}

void graphquery::interact::CFrameLog::Render_Frame() noexcept
{
    ImGui::Begin("Output");


    ImGui::End();
}

void graphquery::interact::CFrameLog::Debug(const std::string &) const noexcept {

}

void graphquery::interact::CFrameLog::Info(const std::string &) const noexcept {

}

void graphquery::interact::CFrameLog::Warning(const std::string &) const noexcept {

}

void graphquery::interact::CFrameLog::Error(const std::string &) const noexcept {

}

void graphquery::interact::CFrameLog::Render_Clear_Button() noexcept {

}

