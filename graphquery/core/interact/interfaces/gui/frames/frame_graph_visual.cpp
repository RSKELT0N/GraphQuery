#include "frame_graph_visual.h"

#include <db/system.h>

namespace
{
    constexpr int32_t node_display_upper_limit = 5;

    [[maybe_unused]] void sparse_node_positions(const int64_t num_vertices) noexcept
    {
        float x                     = 0.0f;
        float y                     = 0.0f;
        static constexpr float dist = 200.0f;

        const int render_c = std::min(static_cast<int64_t>(node_display_upper_limit), num_vertices);
        const int width    = std::log2(render_c);
        for (int i = 0; i < render_c; i++)
        {
            if (i % width == width - 1)
            {
                y += dist;
                x = 0;
            }

            ImVec2 pos(x, y);
            ImNodes::SetNodeGridSpacePos(i, pos);
            x += dist;
        }
    }
} // namespace

graphquery::interact::CFrameGraphVisual::
CFrameGraphVisual(const bool & is_db_loaded, const bool & is_graph_loaded, std::shared_ptr<database::storage::ILPGModel *> graph):
    m_is_db_loaded(is_db_loaded), m_is_graph_loaded(is_graph_loaded), m_graph(std::move(graph))
{
    std::srand(1);
}

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
    ImGui::SetNextWindowSize({ImGui::GetWindowWidth()-300, ImGui::GetWindowHeight()});
    ImNodes::BeginNodeEditor();
    ImNodes::MiniMap(0.1, ImNodesMiniMapLocation_BottomRight);
    ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0, 0, 255, 205));

    if (m_is_db_loaded && m_is_graph_loaded)
    {
        render_nodes();
        render_edges();
    }

    ImNodes::EndNodeEditor();
    ImNodes::PopColorStyle();
}

void
graphquery::interact::CFrameGraphVisual::render_nodes() noexcept
{
    auto vertices = (*m_graph)->get_num_vertices();

    for (int i = 0; i < std::min(static_cast<int64_t>(node_display_upper_limit), vertices); i++)
    {
        auto vertex_i = (*m_graph)->get_vertex(i);
        ImNodes::BeginNode(i);
        ImNodes::BeginNodeTitleBar();
        ImGui::Text("Vertex (%d)", i);
        ImNodes::EndNodeTitleBar();
        ImGui::Text("Neighbours (%u)", vertex_i->outdegree);
        ImNodes::EndNode();
    }
    std::call_once(m_init, sparse_node_positions, vertices);
}

void
graphquery::interact::CFrameGraphVisual::render_edges() noexcept
{
}
