#include "frame_graph_db.h"

#include "db/system.h"

#include <algorithm>

graphquery::interact::CFrameGraphDB::
CFrameGraphDB(const bool & is_db_loaded,
              const bool & is_graph_loaded,
              std::shared_ptr<database::storage::ILPGModel *> graph,
              const std::unordered_map<std::string, database::storage::CDBStorage::SGraph_Entry_t> & graph_table):
    m_is_db_loaded(is_db_loaded), m_is_graph_loaded(is_graph_loaded), m_graph(std::move(graph)), m_graph_table(graph_table)
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
        if(ImGui::BeginChild("#db_data"))
        {
            render_db_info();
            ImGui::NewLine();
            render_graph_table();

            if (m_is_graph_loaded)
                render_loaded_graph();
            ImGui::EndChild();
        }
    }
    else
        ImGui::Text("No current database is loaded.");
}

void
graphquery::interact::CFrameGraphDB::render_db_info() noexcept
{
    if (ImGui::BeginChild("#db_info", {ImGui::GetWindowWidth(), 100}))
    {
        ImGui::SeparatorText("Database Info");
        ImGui::Text("%s", database::_db_storage->get_db_info().c_str());
        ImGui::EndChild();
    }
}

void
graphquery::interact::CFrameGraphDB::render_graph_table() const noexcept
{
    if (ImGui::BeginChild("#db_table", {ImGui::GetWindowWidth(), 200}))
    {
        static constexpr uint8_t columns       = 2;
        static constexpr uint8_t column_width  = 100;
        static constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Hideable;
        ImGui::SeparatorText("Graph Table");

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
                              ImGui::Text("%s", graph.second.graph_name);
                              ImGui::TableSetColumnIndex(1);
                              ImGui::Text("%s", graph.second.graph_type);
                          });
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }
}

void
graphquery::interact::CFrameGraphDB::render_loaded_graph() noexcept
{
    if (ImGui::BeginChild("#graph_info"))
    {
        ImGui::NewLine();
        ImGui::Text("%s", fmt::format("Graph [{}]", (*m_graph)->get_name()).c_str());
        ImGui::Separator();

        ImGui::Text("%s", fmt::format("Vertices: {}", (*m_graph)->get_num_vertices()).c_str());
        ImGui::Text("%s", fmt::format("Edges: {}", (*m_graph)->get_num_edges()).c_str());
        ImGui::Text("%s", fmt::format("Vertex Labels: {}", (*m_graph)->get_num_vertex_labels()).c_str());
        ImGui::Text("%s", fmt::format("Edge Labels: {}", (*m_graph)->get_num_edge_labels()).c_str());
        ImGui::EndChild();
    }
}
