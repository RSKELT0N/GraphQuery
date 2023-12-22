#include "frame_graph_db.h"

#include <algorithm>

#include "db/system.h"

graphquery::interact::CFrameGraphDB::CFrameGraphDB(const bool & is_db_loaded, const std::vector<database::storage::CDBStorage::SGraph_Entry_t> & graph_table): m_is_db_loaded(is_db_loaded), m_graph_table(graph_table)
{
}

graphquery::interact::CFrameGraphDB::~CFrameGraphDB() = default;

void
graphquery::interact::CFrameGraphDB::render_frame() noexcept
{
    if (ImGui::Begin("Graph Database"))
    {
        render_state();
    }
    ImGui::End();
}

void
graphquery::interact::CFrameGraphDB::render_state() noexcept
{
    if (m_is_db_loaded)
    {
        render_db_info();
        ImGui::NewLine();
        render_graph_table();
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
}

void
graphquery::interact::CFrameGraphDB::render_graph_table() noexcept
{
    ImGui::Text("Graph Info");
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
    }
    ImGui::EndTable();
}
