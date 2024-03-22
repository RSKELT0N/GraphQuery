#pragma once

#include "db/storage/dbstorage.h"

#include "interact/interfaces/gui/frame.h"
#include "imfilebrowser.h"

namespace graphquery::interact
{
    class CFrameMenuBar : public IFrame
    {
      public:
        CFrameMenuBar(const bool & is_db_loaded, const bool & is_graph_loaded, const std::unordered_map<std::string, database::storage::CDBStorage::SGraph_Entry_t> & graph_table);
        ~CFrameMenuBar() override;

        void render_frame() noexcept override;

      private:
        void render_close_menu() noexcept;

        void render_create_menu() noexcept;
        void render_open_menu() noexcept;
        void render_open_db() noexcept;
        void render_load_menu() noexcept;
        void render_load_dataset() noexcept;

        void set_create_db_rollback_state(bool) noexcept;
        void render_create_rollback() noexcept;
        void render_create_rollback_entry_data() noexcept;
        void render_create_rollback_button() noexcept;

        void set_create_graph_state(bool) noexcept;
        void render_create_graph() noexcept;
        void render_create_graph_info() noexcept;
        void render_create_graph_name() noexcept;
        void render_create_graph_type() noexcept;
        void render_create_graph_button() noexcept;

        void set_create_db_state(bool) noexcept;
        void render_create_db() noexcept;
        void render_create_db_info() noexcept;
        void render_create_db_location() noexcept;
        void render_create_db_name() noexcept;
        void render_create_db_button() noexcept;

        void set_open_graph_state(bool) noexcept;
        void render_open_graph() noexcept;
        void render_open_graph_list() noexcept;
        void render_open_graph_button() noexcept;

        void set_load_db_rollback_state(bool) noexcept;
        void render_load_rollback() noexcept;
        void render_load_rollback_entry_data() noexcept;
        void render_load_rollback_button() noexcept;

        void setup_db_master_file_explorer() noexcept;
        void setup_db_folder_location_file_explorer() noexcept;
        void setup_dataset_folder_location_explorer() noexcept;

        const bool & m_is_db_loaded;
        const bool & m_is_graph_loaded;
        const std::unordered_map<std::string, database::storage::CDBStorage::SGraph_Entry_t> & m_graph_table;

        ImGui::FileBrowser m_db_master_file_explorer;
        ImGui::FileBrowser m_db_folder_location_explorer;
        ImGui::FileBrowser m_dataset_folder_location_explorer;

        int m_open_graph_choice                 = {};
        int m_load_rollback_choice              = {};
        std::filesystem::path m_created_db_path = {};
        std::string m_created_db_rollback_name  = {};
        std::string m_created_db_name           = {};
        std::string m_created_graph_name        = {};
        std::string m_created_graph_type        = {};
        bool m_is_load_db_rollback_opened       = false;
        bool m_is_create_db_rollback_opened     = false;
        bool m_is_create_db_opened              = false;
        bool m_is_create_graph_opened           = false;
        bool m_is_open_graph_opened             = false;

        static constexpr size_t DB_NAME_SIZE         = 20;
        static constexpr size_t DB_PATH_SIZE         = 100;
        static constexpr size_t CREATE_WINDOW_WIDTH  = 400;
        static constexpr size_t CREATE_WINDOW_HEIGHT = 200;
    }; // namespace graphquery::interact
} // namespace graphquery::interact
