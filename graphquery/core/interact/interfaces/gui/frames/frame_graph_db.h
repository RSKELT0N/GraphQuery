#pragma once

#include "interact/interfaces/gui/frame.h"

namespace graphquery::interact
{
    class CFrameGraphDB : public IFrame
    {
    public:
        CFrameGraphDB() = default;
        ~CFrameGraphDB() override;

    public:
        void Render_Frame() noexcept override;
        void Render_State() noexcept;
        void Render_GraphInfo() noexcept;
    };
}
