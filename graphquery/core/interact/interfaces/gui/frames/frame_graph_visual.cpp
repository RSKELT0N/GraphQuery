#include "frame_graph_visual.h"

#include "db/utils/ring_buffer.h"

#include <db/system.h>

graphquery::interact::CFrameGraphVisual::~CFrameGraphVisual() = default;

void
graphquery::interact::CFrameGraphVisual::render_frame() noexcept
{
    if (ImGui::Begin("Graph Visual"))
    {
        render_grid();
        ImGui::End();
    }
}

void
graphquery::interact::CFrameGraphVisual::render_grid() noexcept
{
    ImNodes::BeginNodeEditor();
    ImNodes::MiniMap(0.15, ImNodesMiniMapLocation_BottomRight);

    if (!database::_db_storage->get_is_graph_loaded())
    {
        render_nodes();
        render_edges();
    }

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
