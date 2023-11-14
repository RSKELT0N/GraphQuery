#include "frame_graph_db.h"
#include "db/system.h"

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
    if(!graphquery::database::_db_storage->IsExistingDBLoaded())
        ImGui::Text("No current database is loaded.");
    else Render_GraphInfo();
}

void
graphquery::interact::CFrameGraphDB::Render_GraphInfo() noexcept
{
    ImGui::Text("%s", graphquery::database::_db_storage->GetDBInfo().c_str());
}
