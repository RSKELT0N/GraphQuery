#pragma once

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#define IMGUI_GL_VERSION "#version 400"
#else
#define IMGUI_GL_VERSION "#version 130"
#endif

#include "GLFW/glfw3.h"
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

		static void initialise_nodes_editor() noexcept;
		void initialise_glfw() noexcept;
		void initialise_imgui() noexcept;
		void initialise_frames() noexcept;

	  private:
		std::vector<std::shared_ptr<IFrame>> m_frames;
		std::unique_ptr<GLFWwindow *> m_window;
		bool m_frame_dock_open = false;
	};
} // namespace graphquery::interact
