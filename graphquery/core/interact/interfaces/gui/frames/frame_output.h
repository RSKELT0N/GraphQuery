#pragma once

#include "db/utils/ring_buffer.hpp"
#include "interact/interfaces/gui/frame.h"
#include "log/logsystem/logsystem.h"

#include <string_view>

namespace graphquery::interact
{
    class CFrameLog final : public IFrame, public logger::ILog
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
        enum EImGuiColour : ImU32
        {
            green  = IM_COL32(0, 255, 0, 255),
            red    = IM_COL32(255, 0, 0, 255),
            yellow = IM_COL32(255, 255, 0, 255),
            blue   = IM_COL32(0, 220, 255, 255),
        };

        void render_log_box() const noexcept;
        void render_log_output() const noexcept;
        [[nodiscard]] static bool is_scroll_at_end() noexcept;

        static constexpr std::size_t RING_BUFFER = 1024;
        database::utils::CRingBuffer<std::pair<ImU32, std::string>, RING_BUFFER> m_buffer;
    };
} // namespace graphquery::interact
