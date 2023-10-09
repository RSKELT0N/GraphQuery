#pragma once

#include "imgui.h"
#include "GLFW/glfw3.h"

namespace graphquery::interact
{
    class IFrame
    {
    public:
        explicit IFrame() = default;
        virtual ~IFrame() = default;

        [[maybe_unused]] virtual void Render_Frame() noexcept = 0;

    protected:
        [[maybe_unused]] ImGuiWindowFlags m_flags{};
    };
}