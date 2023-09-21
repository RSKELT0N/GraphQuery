#include "frame_output.hpp"

graphquery::gui::CFrameLog::CFrameLog()
{
}

graphquery::gui::CFrameLog::~CFrameLog()
{

}

void graphquery::gui::CFrameLog::Render_Frame() noexcept
{
    if(ImGui::Begin("Output"))
    {
    }

    ImGui::End();
}

