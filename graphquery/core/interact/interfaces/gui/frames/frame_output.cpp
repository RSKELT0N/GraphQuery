#include "frame_output.h"

#include <algorithm>

#include "imgui_internal.h"

graphquery::interact::CFrameLog::CFrameLog()
{
	m_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysVerticalScrollbar;
}

void
graphquery::interact::CFrameLog::render_frame() noexcept
{
	if (ImGui::Begin("Output"))
	{
		render_clear_button();
		render_log_output();
	}

	ImGui::End();
}

void
graphquery::interact::CFrameLog::render_clear_button() noexcept
{
	if (ImGui::Button("Clear"))
	{
		m_buffer.clear();
	}
}

void
graphquery::interact::CFrameLog::render_log_box() noexcept
{
	if (ImGui::BeginChild("Logs"))
	{
		render_log_output();

		if (is_scroll_at_end())
			ImGui::SetScrollHereY(1.0F);
	}

	ImGui::EndChild();
}

void
graphquery::interact::CFrameLog::render_log_output() const noexcept
{
	std::size_t size = this->m_buffer.get_current_capacity();
	std::size_t head = this->m_buffer.get_head();

	for (std::size_t i = 0; i < size; i++)
	{
		head = (head + 1) % this->m_buffer.get_buffer_size();
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
		ImGui::TextUnformatted(m_buffer[head].c_str());
		ImGui::PopStyleColor();
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

ImVec4
graphquery::interact::CFrameLog::colourise() const noexcept
{
	return {0, 0, 0, 0};
}

void
graphquery::interact::CFrameLog::debug(std::string_view out) noexcept
{
	m_buffer.add(std::string(out));
}

void
graphquery::interact::CFrameLog::info(std::string_view out) noexcept
{
	m_buffer.add(std::string(out));
}

void
graphquery::interact::CFrameLog::warning(std::string_view out) noexcept
{
	m_buffer.add(std::string(out));
}

void
graphquery::interact::CFrameLog::error(std::string_view out) noexcept
{
	m_buffer.add(std::string(out));
}
