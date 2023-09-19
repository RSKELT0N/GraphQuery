#pragma once

#include "frame.hpp"

namespace graphquery::gui
{
    class CFrameLog : public IFrame
    {
    public:
        CFrameLog();
        ~CFrameLog() override;

    public:
        void Render_Frame() noexcept override;
    };

}
