#pragma once

#include "log/logger.h"
#include "interact/interfaces/gui/frame.h"
#include "db/utils/ring_buffer.h"

namespace graphquery::interact
{
    class CFrameLog : public IFrame, public graphquery::logger::ILog
    {
    public:
        CFrameLog();
        ~CFrameLog() override = default;

        void Render_Frame() noexcept override;

    protected:
        void Debug(const std::string &) noexcept override;
        void Info(const std::string &) noexcept override;
        void Warning(const std::string &) noexcept override;
        void Error(const std::string &)  noexcept override;

    private:
        void Render_Clear_Button() noexcept;
        void Render_Log_Box() noexcept;
        void Render_Log_Output() noexcept;
        [[nodiscard]] static bool Is_Scroll_At_End() noexcept;
        [[nodiscard]] ImVec4 Colourise() const noexcept;

    private:
        static constexpr std::size_t RING_BUFFER = 5;
        database::utils::CRingBuffer<std::string, RING_BUFFER> m_buffer;
    };
}
