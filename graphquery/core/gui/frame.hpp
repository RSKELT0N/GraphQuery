#pragma once

#include <imgui.h>
#include <GLFW/glfw3.h>

namespace graphquery::gui
{
    class IFrame
    {
    public:
        virtual ~IFrame() = default;
        explicit IFrame() = default;

        IFrame(const IFrame &) = delete;
        IFrame(IFrame &&) = delete;
        IFrame & operator=(const IFrame &) = delete;
        IFrame & operator=(IFrame &&) = delete;

    public:
        [[maybe_unused]] virtual void Render_Frame() noexcept = 0;

    protected:
        ImGuiWindowFlags m_flags;
        bool m_is_opened;
    };
}