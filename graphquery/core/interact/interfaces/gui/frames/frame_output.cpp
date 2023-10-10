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

void graphquery::interact::CFrameLog::Render_Log_Output() noexcept
{
    auto buffer_data = m_buffer.Get_Data();
    std::size_t& size = buffer_data.current_capacity;
    std::size_t& head = buffer_data.head;

    for(std::size_t i = 0; i < size; i++)
    {
        std::string ss((*buffer_data.data)[head].c_str(), (*buffer_data.data)[head].c_str() + (*buffer_data.data)[head].length() + 1);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0,255,0,255));
        ImGui::TextUnformatted(ss.c_str());
        ImGui::PopStyleColor();
        head = (head + 1) % buffer_data.data->size();
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
    return ImVec4(0,0,0,0);
}

void graphquery::interact::CFrameLog::Debug(const std::string & out) noexcept
{
    m_buffer.Add_Element(out);
}

void graphquery::interact::CFrameLog::Info(const std::string & out) noexcept
{
    m_buffer.Add_Element(out);
}

void graphquery::interact::CFrameLog::Warning(const std::string & out) noexcept
{
    m_buffer.Add_Element(out);

}

void graphquery::interact::CFrameLog::Error(const std::string & out) noexcept
{
    m_buffer.Add_Element(out);
}

