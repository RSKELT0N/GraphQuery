#include "frame_graph_db.h"

#include <algorithm>

#include "db/system.h"

graphquery::interact::CFrameGraphDB::CFrameGraphDB(const bool & is_db_loaded, const std::vector<database::storage::CDBStorage::SGraph_Entry_t> & graph_table): m_is_db_loaded(is_db_loaded), m_graph_table(graph_table) {}


graphquery::interact::CFrameGraphDB::~CFrameGraphDB() = default;

void
graphquery::interact::CFrameGraphDB::Render_Frame() noexcept
{
    if(ImGui::Begin("Graph Database"))
    {
        Render_State();
    }
    ImGui::End();
}

void
graphquery::interact::CFrameGraphDB::Render_State() noexcept
{
    if(m_is_db_loaded)
    {
        Render_DBInfo();
        ImGui::NewLine();
        Render_GraphTable();
    } else ImGui::Text("No current database is loaded.");

}

void
graphquery::interact::CFrameGraphDB::Render_DBInfo() noexcept
{
    ImGui::Text("Database Info");
    ImGui::Separator();
    ImGui::Text("%s", database::_db_storage->GetDBInfo().c_str());
}

void
graphquery::interact::CFrameGraphDB::Render_GraphTable() noexcept
{
    ImGui::Text("Graph Info");
    ImGui::Separator();

    if (ImGui::BeginTable("##", 2, ImGuiTableFlags_Borders))
    {
        std::for_each(m_graph_table.begin(), m_graph_table.end(), [](const auto & graph) -> void
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Name: %s", graph.graph_name);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("Type: %s", graph.graph_type);
        });
    }
    ImGui::EndTable();
}

