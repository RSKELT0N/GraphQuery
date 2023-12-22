#pragma once

#include "db/storage/dbstorage.h"
#include <vector>

#include "interact/interfaces/gui/frame.h"

namespace graphquery::interact
{
    class CFrameGraphDB : public IFrame
    {
      public:
        CFrameGraphDB(const bool & is_db_loaded, const std::vector<database::storage::CDBStorage::SGraph_Entry_t> & graph_table);
        ~CFrameGraphDB() override;

        void render_frame() noexcept override;

      private:
        void render_state() noexcept;
        void render_db_info() noexcept;
        void render_graph_table() noexcept;

        const bool & m_is_db_loaded;
        const std::vector<database::storage::CDBStorage::SGraph_Entry_t> & m_graph_table;
    };
} // namespace graphquery::interact
