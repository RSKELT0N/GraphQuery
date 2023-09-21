#pragma once

#include <gui/frame.hpp>

namespace graphquery::gui
{
    class CFrameDock : public IFrame
    {
    public:
        CFrameDock(GLFWwindow *, bool);
        ~CFrameDock() override;

    public:
        void Render_Frame() noexcept override;
    private:
        GLFWwindow * m_window;
        ImGuiViewport * m_viewp;
    };



}
