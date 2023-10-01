#pragma once

#include "interact/interfaces/gui/frame.h"

namespace graphquery::interact
{
    class CFrameDock : public IFrame
    {
    public:
        CFrameDock(GLFWwindow *, bool &);
        ~CFrameDock() override;

    public:
        void Render_Frame() noexcept override;
    private:
        GLFWwindow * m_window;
        ImGuiViewport * m_viewp;
        bool & m_is_opened;
    };
}
