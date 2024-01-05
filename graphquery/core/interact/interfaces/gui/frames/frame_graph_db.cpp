#include "frame_graph_db.h"

#include "db/system.h"
#include "models/lpg/lpg.h"

#include <algorithm>
#include <utility>

graphquery::interact::CFrameGraphDB::CFrameGraphDB(const bool & is_db_loaded,
                                                   const bool & is_graph_loaded,
                                                   const std::vector<database::storage::CDBStorage::SGraph_Entry_t> & graph_table):
    m_is_db_loaded(is_db_loaded),
    m_is_graph_loaded(is_graph_loaded), m_graph_table(graph_table)
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
            graph->add_vertex("PERSON", {"First Name", "Skelton"});

        if (ImGui::Button("Add Vertex (0)"))
            graph->add_vertex(0, "PERSON", {});

        if (ImGui::Button("Add Edge"))
            graph->add_edge(0, 1, "PERSON", {});

        if (ImGui::Button("Remove Vertex"))
            graph->rm_vertex(0);

        if (ImGui::Button("Remove Edge"))
            graph->rm_edge(0, 1, "PERSON");
    }
#endif
}

void
graphquery::interact::CFrameGraphDB::render_graph_table() noexcept
{
    ImGui::Text("Graph Table");
    ImGui::Separator();

    if (ImGui::BeginTable("##", 2, ImGuiTableFlags_Borders))
    {
        std::for_each(m_graph_table.begin(),
                      m_graph_table.end(),
                      [](const auto & graph) -> void
                      {
                          ImGui::TableNextRow();
                          ImGui::TableSetColumnIndex(0);
                          ImGui::Text("Name: %s", graph.graph_name);
                          ImGui::TableSetColumnIndex(1);
                          ImGui::Text("Memory-model: %s", graph.graph_type);
                      });
        ImGui::EndTable();
    }
}

void
graphquery::interact::CFrameGraphDB::render_loaded_graph() noexcept
{
    ImGui::NewLine();
    ImGui::Text("%s", fmt::format("Graph [{}]", database::_db_storage->get_graph()->get_name()).c_str());
    ImGui::Separator();

    ImGui::Text("%s", fmt::format("Vertices: {}", database::_db_storage->get_graph()->get_num_vertices()).c_str());
    ImGui::Text("%s", fmt::format("Edges: {}", database::_db_storage->get_graph()->get_num_edges()).c_str());
}
