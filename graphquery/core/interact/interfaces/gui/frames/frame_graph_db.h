#pragma once

#include "db/storage/dbstorage.h"
#include "interact/interfaces/gui/frame.h"

#include <memory>

namespace graphquery::interact
{
    class CFrameGraphDB : public IFrame
    {
      public:
        CFrameGraphDB(const bool & is_db_loaded,
                      const bool & is_graph_loaded,
                      std::shared_ptr<database::storage::ILPGModel *> graph,
                      const std::unordered_map<std::string, database::storage::CDBStorage::SGraph_Entry_t> & graph_table);
        ~CFrameGraphDB() override;

        void render_frame() noexcept override;

      private:
        void render_state() noexcept;
        void render_db_info() noexcept;
        void render_graph_table() const noexcept;
        void render_loaded_graph() noexcept;

        const bool & m_is_db_loaded;
        const bool & m_is_graph_loaded;
        std::shared_ptr<database::storage::ILPGModel *> m_graph;
        const std::unordered_map<std::string, database::storage::CDBStorage::SGraph_Entry_t> & m_graph_table;
    };
} // namespace graphquery::interact
