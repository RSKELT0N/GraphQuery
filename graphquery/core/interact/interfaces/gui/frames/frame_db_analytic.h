#pragma once

#include "db/storage/dbstorage.h"
#include "interact/interfaces/gui/frame.h"

namespace graphquery::interact
{
    class CFrameDBAnalytic : public IFrame
    {
      public:
        CFrameDBAnalytic(const bool & is_db_loaded,
                         const bool & is_graph_loaded,
                         std::shared_ptr<database::storage::ILPGModel *> graph);
        ~CFrameDBAnalytic() override;

        void render_frame() noexcept override;

      private:
        const bool & m_is_db_loaded;
        const bool & m_is_graph_loaded;
        std::shared_ptr<database::storage::ILPGModel *> m_graph;
    };
} // namespace graphquery::interact
