#include "frame_graph_visual.h"

#include "db/utils/ring_buffer.h"

graphquery::interact::CFrameGraphVisual::~CFrameGraphVisual() = default;

void
graphquery::interact::CFrameGraphVisual::render_frame() noexcept
{
    if (ImGui::Begin("Graph Visual"))
    {
        render_grid();
    }
    ImGui::End();
}

void
graphquery::interact::CFrameGraphVisual::render_grid() noexcept
{
    ImNodes::BeginNodeEditor();
    render_nodes();
    render_edges();
    ImNodes::MiniMap(0.15, ImNodesMiniMapLocation_BottomRight);
    ImNodes::EndNodeEditor();
}

void
graphquery::interact::CFrameGraphVisual::render_nodes() noexcept
{
}

void
graphquery::interact::CFrameGraphVisual::render_edges() noexcept
{
}
