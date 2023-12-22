#pragma once

#include "db/utils/ring_buffer.h"
#include "interact/interfaces/gui/frame.h"
#include "log/logsystem/logsystem.h"

#include <string_view>

namespace graphquery::interact
{
	class CFrameLog : public IFrame, public graphquery::logger::ILog
	{
	  public:
		CFrameLog();
		~CFrameLog() override = default;

		void render_frame() noexcept override;

	  protected:
		void debug(std::string_view) noexcept override;
		void info(std::string_view) noexcept override;
		void warning(std::string_view) noexcept override;
		void error(std::string_view) noexcept override;

	  private:
		void render_clear_button() noexcept;
		void render_log_box() noexcept;
		void render_log_output() const noexcept;
		[[nodiscard]] static bool is_scroll_at_end() noexcept;
		[[nodiscard]] ImVec4 colourise() const noexcept;

	  private:
		static constexpr std::size_t RING_BUFFER = 1024;
		database::utils::CRingBuffer<std::string, RING_BUFFER> m_buffer;
	};
} // namespace graphquery::interact
