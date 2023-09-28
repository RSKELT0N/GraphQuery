#pragma once

#include "gui/frame.h"

namespace graphquery::gui
{
    class CFrameMenuBar : public IFrame
    {
    public:
        CFrameMenuBar() = default;
        ~CFrameMenuBar() override;

    public:
        void Render_Frame() noexcept override;
    };



}
