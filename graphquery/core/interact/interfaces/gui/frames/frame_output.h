#pragma once

#include "log/logsystem.h"
#include "interact/interfaces/gui/frame.h"
#include "db/utils/ring_buffer.h"

#include <string_view>

namespace graphquery::interact
{
    class CFrameLog : public IFrame, public graphquery::logger::ILog
    {
    public:
        CFrameLog();
        ~CFrameLog() override = default;

        void Render_Frame() noexcept override;

    protected:
        void Debug(std::string_view) noexcept override;
        void Info(std::string_view) noexcept override;
        void Warning(std::string_view) noexcept override;
        void Error(std::string_view)  noexcept override;

    private:
        void Render_Clear_Button() noexcept;
        void Render_Log_Box() noexcept;
        void Render_Log_Output() const noexcept;
        [[nodiscard]] static bool Is_Scroll_At_End() noexcept;
        [[nodiscard]] ImVec4 Colourise() const noexcept;

    private:
        static constexpr std::size_t RING_BUFFER = 1024;
        database::utils::CRingBuffer<std::string, RING_BUFFER> m_buffer;
    };
}
