#pragma once

#include <vector>
#include <db/storage/dbstorage.h>

#include "interact/interfaces/gui/frame.h"

namespace graphquery::interact
{
    class CFrameGraphDB : public IFrame
    {
    public:
        CFrameGraphDB(const bool & is_db_loaded, const std::vector<database::storage::CDBStorage::SGraph_Entry_t> & graph_table);
        ~CFrameGraphDB() override;

        void Render_Frame() noexcept override;

    private:
        void Render_State() noexcept;
        void Render_DBInfo() noexcept;
        void Render_GraphTable() noexcept;

        const bool & m_is_db_loaded;
        const std::vector<database::storage::CDBStorage::SGraph_Entry_t> & m_graph_table;
    };
}
