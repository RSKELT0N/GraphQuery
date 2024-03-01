#include "frame_graph_visual.h"

#include <db/system.h>

graphquery::interact::CFrameGraphVisual::
CFrameGraphVisual(const bool & is_db_loaded, const bool & is_graph_loaded, std::shared_ptr<database::storage::ILPGModel *> graph):
    m_is_db_loaded(is_db_loaded), m_is_graph_loaded(is_graph_loaded), m_graph(std::move(graph))
{
}

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

    if (m_is_db_loaded && m_is_graph_loaded)
    {
        render_nodes();
        render_edges();
    }

    ImNodes::EndNodeEditor();
}

void
graphquery::interact::CFrameGraphVisual::render_nodes() noexcept
{
    auto vertices = (*m_graph)->get_num_vertices();

    for(int64_t i = 0; i < std::min(50LL, vertices); i++)
    {
        auto vertex_i = (*m_graph)->get_vertex(i);

        ImNodes::BeginNode(i);
        ImGui::Text("Vertex (%lld)", i);
        ImGui::Text("Neighbours (%u)", vertex_i->neighbour_c);
        ImNodes::EndNode();
    }
}

void
graphquery::interact::CFrameGraphVisual::render_edges() noexcept
{
}
