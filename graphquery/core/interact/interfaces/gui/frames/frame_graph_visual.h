#pragma once

#include "interact/interfaces/gui/frame.h"

#include "imgui.h"
#include "imnodes.h"

namespace graphquery::interact
{
	class CFrameGraphVisual : public IFrame
	{
	  public:
		CFrameGraphVisual() = default;
		~CFrameGraphVisual() override;

	  public:
		void render_frame() noexcept override;

	  private:
		void render_grid() noexcept;
		void render_nodes() noexcept;
		void render_edges() noexcept;
	};
} // namespace graphquery::interact
