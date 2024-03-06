#pragma once

#include "interact/interfaces/gui/frame.h"

#include "imgui.h"
#include "imnodes.h"
#include "db/storage/graph_model.h"

namespace graphquery::interact
{
    class CFrameGraphVisual : public IFrame
    {
      public:
        CFrameGraphVisual(const bool & is_db_loaded, const bool & is_graph_loaded, std::shared_ptr<database::storage::ILPGModel *> graph);
        ~CFrameGraphVisual() override;

        void render_frame() noexcept override;

      private:
        void render_grid() noexcept;
        void render_nodes() noexcept;
        void render_edges() noexcept;

        std::once_flag m_init;
        const bool & m_is_db_loaded;
        const bool & m_is_graph_loaded;
        std::shared_ptr<database::storage::ILPGModel *> m_graph;
    };
} // namespace graphquery::interact
