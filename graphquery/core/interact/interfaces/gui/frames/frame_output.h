#pragma once

#include "interact/interfaces/gui/frame.h"
#include "log/logger.h"

namespace graphquery::interact
{
class CFrameLog : public IFrame, public graphquery::logger::ILog
    {
    public:
        CFrameLog();
        ~CFrameLog() override = default;

    public:
        void Render_Frame() noexcept override;

protected:
        void Debug(const std::string &) const noexcept override;
        void Info(const std::string &) const noexcept override;
        void Warning(const std::string &) const noexcept override;
        void Error(const std::string &) const noexcept override;

    private:
        void Render_Clear_Button() noexcept;
    };

}
