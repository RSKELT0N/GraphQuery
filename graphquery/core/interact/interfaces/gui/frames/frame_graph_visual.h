#pragma once

#include "interact/interfaces/gui/frame.h"

#include "imnodes.h"
#include "imgui.h"

namespace graphquery::interact
{
    class CFrameGraphVisual : public IFrame
    {
    public:
        CFrameGraphVisual() = default;
        ~CFrameGraphVisual() override;

    public:
        void Render_Frame() noexcept override;
    private:
        void Render_Grid() noexcept;
        void Render_Nodes() noexcept;
        void Render_Edges() noexcept;
    };
}
