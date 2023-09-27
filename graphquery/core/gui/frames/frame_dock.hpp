#pragma once

#include <gui/frame.hpp>

namespace graphquery::gui
{
    class CFrameDock : public IFrame
    {
    public:
        CFrameDock(GLFWwindow *, bool &);
        ~CFrameDock() override;

    public:
        void Render_Frame() noexcept override;
    private:
        [[maybe_unused]] GLFWwindow * m_window;
        [[maybe_unused]] ImGuiViewport * m_viewp;
        [[maybe_unused]] bool & m_is_opened;
    };
}
