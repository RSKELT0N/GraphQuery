#pragma once

#include "interact/interfaces/gui/frame.h"

namespace graphquery::interact
{
	class CFrameDock : public IFrame
	{
	  public:
		CFrameDock(bool &);
		~CFrameDock() override;

	  public:
		void render_frame() noexcept override;

	  private:
		ImGuiViewport * m_viewp;
		bool & m_is_opened;
	};
} // namespace graphquery::interact
