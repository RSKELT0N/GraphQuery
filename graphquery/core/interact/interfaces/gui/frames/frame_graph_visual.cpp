#include "frame_graph_visual.h"

#include "db/system.h"

#include "fmt/format.h"

graphquery::interact::CFrameGraphVisual::~CFrameGraphVisual() = default;

void graphquery::interact::CFrameGraphVisual::Render_Frame() noexcept
{
    if(ImGui::Begin("Graph Visual"))
    {
        Render_Grid();
    }
    ImGui::End();
}

void graphquery::interact::CFrameGraphVisual::Render_Grid() noexcept
{
    ImNodes::BeginNodeEditor();
    Render_Nodes();
    Render_Edges();
    ImNodes::EndNodeEditor();
}

void graphquery::interact::CFrameGraphVisual::Render_Nodes() noexcept
{
    for(int i = 0; i < 10; i++)
    {
        ImNodes::BeginNode(i);

        ImNodes::BeginNodeTitleBar();
        ImGui::Text("Node(%d)", i);
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginOutputAttribute(i);
        ImGui::Text("Opin %d", i);
        ImNodes::EndOutputAttribute();

        ImNodes::BeginInputAttribute(i*2);
        ImGui::Text("Ipin %d", i*2);
        ImNodes::EndInputAttribute();

        ImNodes::EndNode();

        if(ImNodes::IsNodeSelected(i))
        {
            graphquery::database::_log_system->Info(fmt::format("Node Selected ({})", i));
        }
    }
}

void graphquery::interact::CFrameGraphVisual::Render_Edges() noexcept
{

}


