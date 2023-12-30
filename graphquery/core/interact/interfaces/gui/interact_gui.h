#pragma once

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#define IMGUI_GL_VERSION "#version 150"
#else
#define IMGUI_GL_VERSION "#version 130"
#endif

#include "imgui.h"
#include "imnodes.h"
#include "frame.h"

#include "interact/interact.h"

#include <memory>
#include <vector>

namespace graphquery::interact
{
    class CInteractGUI final : public IInteract
    {
      public:
        explicit CInteractGUI();
        ~CInteractGUI() override = default;

        void clean_up() noexcept override;

      private:
        void render() noexcept override;

        void render_frames() noexcept;
        void on_update() noexcept;

        void initialise_nodes_editor() noexcept;
        void initialise_glfw() noexcept;
        void initialise_imgui() noexcept;
        void initialise_frames() noexcept;

      private:
        std::vector<std::shared_ptr<IFrame>> m_frames;

        GLFWwindow * m_window              = {};
        ImGuiContext * m_imgui_context     = {};
        ImNodesContext * m_imnodes_context = {};
        bool m_frame_dock_open             = false;
    };
} // namespace graphquery::interact
