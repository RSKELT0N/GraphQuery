#pragma once

#include <gui/frame.hpp>

namespace graphquery::gui
{
    class CFrameLog : public IFrame
    {
    public:
        CFrameLog();
        ~CFrameLog() override;

    public:
        void Render_Frame() noexcept override;

    private:
        void Render_Clear_Button() noexcept;
    };

}
