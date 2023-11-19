#pragma once

#include <db/storage/dbstorage.h>

#include "interact/interfaces/gui/frame.h"
#include "imfilebrowser.h"

namespace graphquery::interact
{
    class CFrameMenuBar : public IFrame
    {
    public:
        CFrameMenuBar(const bool & is_db_loaded, const std::vector<database::storage::CDBStorage::SGraph_Entry_t> & graph_table);
        ~CFrameMenuBar() override;

        void Render_Frame() noexcept override;

    private:
        void Render_DBMenu() noexcept;

        void Render_CreateMenu() noexcept;
        void Render_OpenMenu() noexcept;
        void Render_OpenDB() noexcept;

        void SetCreateGraphState(bool) noexcept;
        void Render_CreateGraph() noexcept;
        void Render_CreateGraphInfo() noexcept;
        void Render_CreateGraphName() noexcept;
        void Render_CreateGraphType() noexcept;
        void Render_CreateGraphButton() noexcept;

        void SetCreateDBState(bool) noexcept;
        void Render_CreateDB() noexcept;
        void Render_CreateDBInfo() noexcept;
        void Render_CreateDBLocation() noexcept;
        void Render_CreateDBName() noexcept;
        void Render_CreateDBButton() noexcept;

        void SetOpenGraphState(bool) noexcept;
        void Render_OpenGraph() noexcept;
        void Render_OpenGraphList() noexcept;
        void Render_OpenGraphButton() noexcept;

        const bool & m_is_db_loaded;
        [[maybe_unused]] const std::vector<database::storage::CDBStorage::SGraph_Entry_t> & m_graph_table;

        ImGui::FileBrowser m_db_master_file_explorer;
        ImGui::FileBrowser m_db_folder_location_explorer;

        int m_open_graph_choice = {};
        std::string m_created_db_path = {};
        std::string m_created_db_name = {};
        std::string m_created_graph_name = {};
        std::string m_created_graph_type = {};
        bool m_is_create_db_opened = false;
        bool m_is_create_graph_opened = false;
        bool m_is_open_graph_opened = false;

        static constexpr size_t DB_NAME_SIZE = 20;
        static constexpr size_t DB_PATH_SIZE = 100;
        static constexpr size_t CREATE_WINDOW_WIDTH = 400;
        static constexpr size_t CREATE_WINDOW_HEIGHT = 200;
    };
}
