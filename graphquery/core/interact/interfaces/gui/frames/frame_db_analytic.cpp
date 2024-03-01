#include "frame_db_analytic.h"

#include "frame_db_query.h"

graphquery::interact::CFrameDBAnalytic::
CFrameDBAnalytic(const bool & is_db_loaded, const bool & is_graph_loaded, std::shared_ptr<database::storage::ILPGModel *> graph):
    m_is_db_loaded(is_db_loaded), m_is_graph_loaded(is_graph_loaded), m_graph(std::move(graph))
{
}

graphquery::interact::CFrameDBAnalytic::~
CFrameDBAnalytic()
{
}

void
graphquery::interact::CFrameDBAnalytic::render_frame() noexcept
{
    if(m_is_db_loaded && m_is_graph_loaded)
    {
        if (ImGui::Begin("Analytic"))
        {
            ImGui::Text("No current database is loaded.");
            ImGui::End();
        }
    }
}
