#pragma once

#include "interact/interfaces/gui/frame.h"

namespace graphquery::interact
{
    class CFrameDock : public IFrame
    {
    public:
        CFrameDock(bool &);
        ~CFrameDock() override;

    public:
        void Render_Frame() noexcept override;
    private:
        ImGuiViewport * m_viewp;
        bool & m_is_opened;
    };
}
