#include "frame_graph_db.h"

#include "db/system.h"

#include <algorithm>
#include <utility>

graphquery::interact::CFrameGraphDB::CFrameGraphDB(const bool & is_db_loaded,
                                                   const bool & is_graph_loaded,
                                                   std::shared_ptr<database::storage::ILPGModel *> graph,
                                                   const std::vector<database::storage::CDBStorage::SGraph_Entry_t> & graph_table):
    m_is_db_loaded(is_db_loaded),
    m_is_graph_loaded(is_graph_loaded), m_graph(std::move(graph)), m_graph_table(graph_table)
{
}

graphquery::interact::CFrameGraphDB::~CFrameGraphDB() = default;

void
graphquery::interact::CFrameGraphDB::render_frame() noexcept
{
    if (ImGui::Begin("Graph Database"))
    {
        render_state();
        ImGui::End();
    }
}

void
graphquery::interact::CFrameGraphDB::render_state() noexcept
{
    if (m_is_db_loaded)
    {
        render_db_info();
        ImGui::NewLine();
        render_graph_table();

        if (m_is_graph_loaded)
            render_loaded_graph();
    }
    else
        ImGui::Text("No current database is loaded.");
}

void
graphquery::interact::CFrameGraphDB::render_db_info() noexcept
{
    ImGui::Text("Database Info");
    ImGui::Separator();
    ImGui::Text("%s", database::_db_storage->get_db_info().c_str());

#ifndef NDEBUG
    if (m_is_graph_loaded)
    {
        const auto graph = database::_db_storage->get_graph();
        if (ImGui::Button("Add Vertex"))
            for (uint64_t i = 0; i < 10; i++)
                (*m_graph)->add_vertex("PERSON", {});

        if (ImGui::Button("Add Vertex (0)"))
            (*m_graph)->add_vertex(0, "PERSON", {});

        if (ImGui::Button("Add Edge"))
            (*m_graph)->add_edge(0, 1, "KNOWS", {});

        if (ImGui::Button("Remove Vertex"))
            (*m_graph)->rm_vertex(0);

        if (ImGui::Button("Remove Edge"))
            (*m_graph)->rm_edge(0, 1, "PERSON");
    }
#endif
}

void
graphquery::interact::CFrameGraphDB::render_graph_table() noexcept
{
    static constexpr uint8_t columns       = 2;
    static constexpr uint8_t column_width  = 100;
    static constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Hideable;
    ImGui::Text("Graph Table");

    ImGui::Separator();
    if (ImGui::BeginTable("##", columns, flags))
    {
        ImGui::TableSetupColumn("Graph Name", ImGuiTableColumnFlags_WidthFixed, column_width);
        ImGui::TableSetupColumn("Graph Model", ImGuiTableColumnFlags_WidthFixed, column_width);
        ImGui::TableHeadersRow();
        std::for_each(m_graph_table.begin(),
                      m_graph_table.end(),
                      [](const auto & graph) -> void
                      {
                          ImGui::TableNextRow();
                          ImGui::TableSetColumnIndex(0);
                          ImGui::Text("%s", graph.graph_name);
                          ImGui::TableSetColumnIndex(1);
                          ImGui::Text("%s", graph.graph_type);
                      });
        ImGui::EndTable();
    }
}

void
graphquery::interact::CFrameGraphDB::render_loaded_graph() noexcept
{
    ImGui::NewLine();
    ImGui::Text("%s", fmt::format("Graph [{}]", (*m_graph)->get_name()).c_str());
    ImGui::Separator();

    ImGui::Text("%s", fmt::format("Vertices: {}", (*m_graph)->get_num_vertices()).c_str());
    ImGui::Text("%s", fmt::format("Edges: {}", (*m_graph)->get_num_edges()).c_str());
}
