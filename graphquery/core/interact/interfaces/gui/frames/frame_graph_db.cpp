#include "frame_graph_db.h"
#include "db/system.h"

graphquery::interact::CFrameGraphDB::~CFrameGraphDB() = default;

void graphquery::interact::CFrameGraphDB::Render_Frame() noexcept
{
    if(ImGui::Begin("Graph Database"))
    {
        Render_State();
    }
    ImGui::End();
}

void graphquery::interact::CFrameGraphDB::Render_State() noexcept
{
    if(!graphquery::database::_storage->IsExistingDBLoaded())
    {
        ImGui::Text("No current database is loaded.");
    }
    Render_GraphInfo();
}

void graphquery::interact::CFrameGraphDB::Render_GraphInfo() noexcept
{

}
