#pragma once

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#define IMGUI_GL_VERSION "#version 400"
#else
#define IMGUI_GL_VERSION "#version 130"
#endif

#include "interact/interact.h"
#include "frame.h"
#include "imgui.h"
#include "GLFW/glfw3.h"

#include <memory>
#include <vector>

namespace graphquery::interact
{
    class CInteractGUI final : public IInteract
    {
    public:
        explicit CInteractGUI();
        ~CInteractGUI() override = default;

    private:
        void Render() noexcept override;

        void Render_Frames() noexcept;
        void Clean_Up() noexcept;
        void On_Update() noexcept;

        static void Initialise_Nodes_Editor() noexcept;
        void Initialise_GLFW() noexcept;
        void Initialise_IMGUI() noexcept;
        void Initialise_Frames() noexcept;

    private:
        std::vector<std::shared_ptr<IFrame> > m_frames;
        std::unique_ptr<GLFWwindow *> m_window;

        [[maybe_unused]] bool m_frame_dock_open = false;
        [[maybe_unused]] bool m_frame_output_open = true;

    };
}
