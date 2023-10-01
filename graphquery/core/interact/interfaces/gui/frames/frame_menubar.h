#pragma once

#include "interact/interfaces/gui/frame.h"

namespace graphquery::interact
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
