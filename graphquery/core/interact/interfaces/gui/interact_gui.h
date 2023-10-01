#pragma once

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#define IMGUI_GL_VERSION "#version 400"
#else
#define IMGUI_GL_VERSION "#version 130"
#endif

#include "../../interact.h"
#include "frame.h"
#include "imgui.h"
#include "./GLFW/glfw3.h"

#include <memory>
#include <vector>

namespace graphquery::interact
{
    class CInteractGUI final : public IInteract
    {
    public:
        explicit CInteractGUI();
        ~CInteractGUI() override = default;

    public:
        int Initialise() noexcept override;
        void Render() noexcept override;

    private:
        [[nodiscard]] static int Initialise_Nodes_Editor() noexcept;

    private:
        void Render_Frames() noexcept;
        void Clean_Up() noexcept;
        void On_Update() noexcept;

        [[nodiscard]] int Initialise_GLFW() noexcept;
        [[nodiscard]] int Initialise_IMGUI() noexcept;
        [[nodiscard]] int Initialise_Frames() noexcept;


    private:
        std::vector<std::unique_ptr<IFrame *> > m_frames;
        std::unique_ptr<GLFWwindow *> m_window;

        bool m_frame_dock_open = false;
        bool m_frame_output_open = true;

    };
}
