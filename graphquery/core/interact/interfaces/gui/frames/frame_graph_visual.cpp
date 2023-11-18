#include "frame_graph_visual.h"

#include "db/utils/ring_buffer.h"
#include "fmt/xchar.h"

graphquery::interact::CFrameGraphVisual::~CFrameGraphVisual() = default;

void
graphquery::interact::CFrameGraphVisual::Render_Frame() noexcept
{
    if(ImGui::Begin("Graph Visual"))
    {
        Render_Grid();
    }
    ImGui::End();
}

void
graphquery::interact::CFrameGraphVisual::Render_Grid() noexcept
{
    ImNodes::BeginNodeEditor();
    Render_Nodes();
    Render_Edges();
    ImNodes::MiniMap(0.15, ImNodesMiniMapLocation_BottomRight);
    ImNodes::EndNodeEditor();
}

void
graphquery::interact::CFrameGraphVisual::Render_Nodes() noexcept
{

}

void
graphquery::interact::CFrameGraphVisual::Render_Edges() noexcept
{

}


