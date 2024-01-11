#include "frame_output.h"

#include "imgui_internal.h"

graphquery::interact::CFrameLog::
CFrameLog()
{
    m_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysVerticalScrollbar;
}

void
graphquery::interact::CFrameLog::render_frame() noexcept
{
    if (ImGui::Begin("Output"))
    {
        if (ImGui::Button("Clear"))
            m_buffer.clear();

        render_log_output();
        ImGui::End();
    }
}

void
graphquery::interact::CFrameLog::render_log_box() const noexcept
{
    if (ImGui::BeginChild("Logs"))
    {
        render_log_output();

        if (is_scroll_at_end())
            ImGui::SetScrollHereY(1.0F);
        ImGui::EndChild();
    }
}

bool
graphquery::interact::CFrameLog::is_scroll_at_end() noexcept
{
    if (ImGuiWindow const * window = ImGui::GetCurrentWindow())
    {
        return ((window->Scroll.y) >= window->ScrollMax.y);
    }
    return false;
}

void
graphquery::interact::CFrameLog::render_log_output() const noexcept
{
    const std::size_t size = this->m_buffer.get_current_capacity();
    std::size_t head       = this->m_buffer.get_head();

    for (std::size_t i = 0; i < size; i++)
    {
        head = (head + 1) % this->m_buffer.get_buffer_size();
        ImGui::PushStyleColor(ImGuiCol_Text, m_buffer[head].first);
        ImGui::TextUnformatted(m_buffer[head].second.c_str());
        ImGui::PopStyleColor();
    }
}

void
graphquery::interact::CFrameLog::debug(const std::string_view out) noexcept
{
    m_buffer.add({EImGuiColour::blue, std::string(out)});
}

void
graphquery::interact::CFrameLog::info(const std::string_view out) noexcept
{
    m_buffer.add({EImGuiColour::green, std::string(out)});
}

void
graphquery::interact::CFrameLog::warning(const std::string_view out) noexcept
{
    m_buffer.add({EImGuiColour::yellow, std::string(out)});
}

void
graphquery::interact::CFrameLog::error(const std::string_view out) noexcept
{
    m_buffer.add({EImGuiColour::red, std::string(out)});
}
