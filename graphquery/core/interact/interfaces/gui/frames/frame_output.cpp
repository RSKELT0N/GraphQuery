#include <algorithm>
#include "frame_output.h"
#include "imgui_internal.h"

graphquery::interact::CFrameLog::CFrameLog()
{
    m_flags = ImGuiWindowFlags_NoCollapse |
              ImGuiWindowFlags_AlwaysVerticalScrollbar;
}

void graphquery::interact::CFrameLog::Render_Frame() noexcept
{
    if(ImGui::Begin("Output"))
    {
        Render_Clear_Button();
        Render_Log_Output();
    }

    ImGui::End();
}

void graphquery::interact::CFrameLog::Render_Clear_Button() noexcept
{
    if(ImGui::Button("Clear"))
    {
        m_buffer.Clear();
    }
}

void graphquery::interact::CFrameLog::Render_Log_Box() noexcept
{
    if(ImGui::BeginChild("Logs"))
    {
        Render_Log_Output();

        if(Is_Scroll_At_End()) ImGui::SetScrollHereY(1.0F);
    }

    ImGui::EndChild();
}

void graphquery::interact::CFrameLog::Render_Log_Output() const noexcept
{
    std::size_t size = this->m_buffer.Get_Current_Capacity();
    std::size_t head = this->m_buffer.Get_Head();

    for(std::size_t i = 0; i < size; i++)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0,255,0,255));
        ImGui::TextUnformatted(m_buffer[head].c_str());
        ImGui::PopStyleColor();
        head = (head + 1) % this->m_buffer.Get_BufferSize();
    }
}

bool graphquery::interact::CFrameLog::Is_Scroll_At_End() noexcept
{
    if (ImGuiWindow const *window = ImGui::GetCurrentWindow()) {
        return (window->Scroll.y >= window->ScrollMax.y);
    }
    return false;
}

ImVec4 graphquery::interact::CFrameLog::Colourise() const noexcept
{
    return {0,0,0,0};
}

void graphquery::interact::CFrameLog::Debug(std::string_view out) noexcept
{
    m_buffer.Add(std::string(out));
}

void graphquery::interact::CFrameLog::Info(std::string_view out) noexcept
{
    m_buffer.Add(std::string(out));
}

void graphquery::interact::CFrameLog::Warning(std::string_view out) noexcept
{
    m_buffer.Add(std::string(out));

}

void graphquery::interact::CFrameLog::Error(std::string_view out) noexcept
{
    m_buffer.Add(std::string(out));
}

