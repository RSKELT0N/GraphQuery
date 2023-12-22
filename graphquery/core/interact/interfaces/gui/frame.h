#pragma once

#include "GLFW/glfw3.h"
#include "imgui.h"

namespace graphquery::interact
{
	class IFrame
	{
	  public:
		explicit IFrame() = default;
		virtual ~IFrame() = default;

		[[maybe_unused]] virtual void render_frame() noexcept = 0;

	  protected:
		ImGuiWindowFlags m_flags {};
	};
} // namespace graphquery::interact