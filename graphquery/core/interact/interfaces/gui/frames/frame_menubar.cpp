#include "frame_menubar.h"

#include "db/system.h"
#include "fmt/format.h"
#include "imgui_stdlib.h"

#include <future>

graphquery::interact::CFrameMenuBar::~CFrameMenuBar() = default;

graphquery::interact::CFrameMenuBar::
CFrameMenuBar(const bool & is_db_loaded, const bool & is_graph_loaded, const std::unordered_map<std::string, database::storage::CDBStorage::SGraph_Entry_t> & graph_table):
    m_is_db_loaded(is_db_loaded), m_is_graph_loaded(is_graph_loaded), m_graph_table(graph_table)
{
    setup_db_master_file_explorer();
    setup_db_folder_location_file_explorer();
    setup_dataset_folder_location_explorer();
}

void
graphquery::interact::CFrameMenuBar::setup_db_master_file_explorer() noexcept
{
    this->m_db_master_file_explorer = ImGui::FileBrowser(ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_CreateNewDir);

    this->m_db_master_file_explorer.SetTitle("Open Database");
    this->m_db_master_file_explorer.SetTypeFilters({".gdb"});
}

void
graphquery::interact::CFrameMenuBar::setup_db_folder_location_file_explorer() noexcept
{
    this->m_db_folder_location_explorer = ImGui::FileBrowser(ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_CreateNewDir | ImGuiFileBrowserFlags_SelectDirectory);
    this->m_db_folder_location_explorer.SetTitle("Select Location Path");
}

void
graphquery::interact::CFrameMenuBar::setup_dataset_folder_location_explorer() noexcept
{
    this->m_dataset_folder_location_explorer = this->m_dataset_folder_location_explorer =
        ImGui::FileBrowser(ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_CreateNewDir | ImGuiFileBrowserFlags_SelectDirectory);
    this->m_dataset_folder_location_explorer.SetTitle("Select Dataset Path");
}

void
graphquery::interact::CFrameMenuBar::render_frame() noexcept
{
    if (ImGui::BeginMainMenuBar())
    {
        render_create_menu();
        render_open_menu();
        render_load_menu();
        render_close_menu();
        ImGui::EndMainMenuBar();
    }

    render_create_db();
    render_create_graph();
    render_create_rollback();
    render_open_db();
    render_open_graph();
    render_load_dataset();
    render_load_rollback();
}

void
graphquery::interact::CFrameMenuBar::render_close_menu() noexcept
{
    if (m_is_db_loaded && ImGui::BeginMenu("Close"))
    {
        if (ImGui::MenuItem("Database"))
            database::_db_storage->close();

        if (m_is_graph_loaded)
            if (ImGui::MenuItem("Graph"))
                database::_db_storage->close_graph();

        ImGui::EndMenu();
    }
}

void
graphquery::interact::CFrameMenuBar::render_create_menu() noexcept
{
    if (ImGui::BeginMenu("Create"))
    {
        if (ImGui::MenuItem("Database"))
            set_create_db_state(true);

        if (m_is_db_loaded && ImGui::MenuItem("Graph"))
            set_create_graph_state(true);

        if (m_is_graph_loaded && ImGui::MenuItem("Rollback"))
            set_create_db_rollback_state(true);

        ImGui::EndMenu();
    }
}

void
graphquery::interact::CFrameMenuBar::render_open_menu() noexcept
{
    if (ImGui::BeginMenu("Open", "Ctrl+o"))
    {
        if (ImGui::MenuItem("Database"))
            this->m_db_master_file_explorer.Open();

        if (m_is_db_loaded && ImGui::MenuItem("Graph"))
            set_open_graph_state(true);

        ImGui::EndMenu();
    }
}

void
graphquery::interact::CFrameMenuBar::render_open_db() noexcept
{
    m_db_master_file_explorer.Display();

    if (m_db_master_file_explorer.HasSelected())
    {
        const std::filesystem::path db_master_file_path = m_db_master_file_explorer.GetSelected();
        const std::filesystem::path root_path           = db_master_file_path.parent_path().parent_path();

        database::_db_storage->init(root_path, db_master_file_path.stem().string());
        m_db_master_file_explorer.ClearSelected();
    }
}

void
graphquery::interact::CFrameMenuBar::render_load_menu() noexcept
{
    if (m_is_graph_loaded && ImGui::BeginMenu("Load", "Ctrl+d"))
    {
        if (ImGui::MenuItem("Dataset"))
            this->m_dataset_folder_location_explorer.Open();

        if (ImGui::MenuItem("Rollback"))
            set_load_db_rollback_state(true);

        ImGui::EndMenu();
    }
}

void
graphquery::interact::CFrameMenuBar::render_load_dataset() noexcept
{
    m_dataset_folder_location_explorer.Display();

    if (m_dataset_folder_location_explorer.HasSelected())
    {
        const std::filesystem::path dataset_folder_path = m_dataset_folder_location_explorer.GetSelected();

        std::thread(&database::storage::CDBStorage::load_dataset, database::_db_storage.get(), dataset_folder_path).detach();
        m_dataset_folder_location_explorer.ClearSelected();
    }
}

void
graphquery::interact::CFrameMenuBar::set_create_db_rollback_state(const bool state) noexcept
{
    this->m_is_create_db_rollback_opened = state;
}

void
graphquery::interact::CFrameMenuBar::render_create_rollback() noexcept
{
    if (this->m_is_create_db_rollback_opened)
        ImGui::OpenPopup("Create Rollback");

    ImGui::SetNextWindowSize(ImVec2 {CREATE_WINDOW_WIDTH, CREATE_WINDOW_HEIGHT});
    if (ImGui::BeginPopupModal("Create Rollback", &this->m_is_create_db_rollback_opened, ImGuiWindowFlags_NoResize))
    {
        if (ImGui::BeginChild("Create Rollback info"))
        {
            ImGui::Text("Complete all fields:");
            ImGui::Separator();

            render_create_rollback_entry_data();
            render_create_rollback_button();
            ImGui::EndChild();
        }
        ImGui::EndPopup();
    }
}

void
graphquery::interact::CFrameMenuBar::render_create_rollback_entry_data() noexcept
{
    ImGui::Text("Name: ");
    ImGui::InputText("##_rollbackName", &this->m_created_db_rollback_name, ImGuiInputTextFlags_CharsNoBlank);
    ImGui::Dummy(ImVec2(0.0f, 20.0f));
}

void
graphquery::interact::CFrameMenuBar::render_create_rollback_button() noexcept
{
    if (ImGui::Button("Create Rollback"))
    {
        if (m_created_db_rollback_name.empty() || m_created_db_rollback_name.length() > database::storage::CFG_GRAPH_ROLLBACK_NAME_LENGTH / 2)
        {
            database::_log_system->warning(fmt::format("Name cannot be empty or larger than "
                                                       "configured "
                                                       "size () to create a rollback entry",
                                                       database::storage::CFG_GRAPH_ROLLBACK_NAME_LENGTH / 2));
            return;
        }
        ImGui::CloseCurrentPopup();
        set_create_db_rollback_state(false);

        std::thread(&database::storage::ILPGModel::create_rollback, *database::_db_graph.get(), this->m_created_db_rollback_name).detach();

        m_created_db_rollback_name = "";
    }
}

void
graphquery::interact::CFrameMenuBar::render_create_graph() noexcept
{
    if (this->m_is_create_graph_opened)
        ImGui::OpenPopup("Create Graph");

    ImGui::SetNextWindowSize(ImVec2 {CREATE_WINDOW_WIDTH, CREATE_WINDOW_HEIGHT});
    if (ImGui::BeginPopupModal("Create Graph", &this->m_is_create_graph_opened, ImGuiWindowFlags_NoResize))
    {
        render_create_graph_info();
        ImGui::EndPopup();
    }
}

void
graphquery::interact::CFrameMenuBar::render_create_graph_info() noexcept
{
    if (ImGui::BeginChild("Graph Info"))
    {
        ImGui::Text("Complete all fields:");
        ImGui::Separator();

        render_create_graph_name();
        render_create_graph_type();
        render_create_graph_button();
        ImGui::EndChild();
    }
}

void
graphquery::interact::CFrameMenuBar::render_create_graph_name() noexcept
{
    ImGui::Text("Name: ");
    ImGui::InputText("##_graphName", &this->m_created_graph_name, ImGuiInputTextFlags_CharsNoBlank);
}

void
graphquery::interact::CFrameMenuBar::render_create_graph_type() noexcept
{
    ImGui::Text("Model Type (shared library in lib/): ");
    ImGui::InputText("##_memorymodel", &this->m_created_graph_type, ImGuiInputTextFlags_CharsNoBlank);
}

void
graphquery::interact::CFrameMenuBar::render_create_graph_button() noexcept
{
    if (ImGui::Button("Create Graph"))
    {
        if ((m_created_graph_name.empty() || m_created_graph_name.length() > database::storage::CFG_GRAPH_NAME_LENGTH) ||
            (m_created_graph_type.empty() || m_created_graph_type.length() > database::storage::CFG_GRAPH_MODEL_TYPE_LENGTH))
        {
            database::_log_system->warning(fmt::format("Either the name or type cannot be empty or larger than "
                                                       "configured "
                                                       "size (Name: {}, Memory Model: {}) to create a graph",
                                                       database::storage::CFG_GRAPH_NAME_LENGTH,
                                                       database::storage::CFG_GRAPH_MODEL_TYPE_LENGTH));
            return;
        }
        ImGui::CloseCurrentPopup();
        set_create_graph_state(false);

        std::thread(&database::storage::CDBStorage::create_graph, database::_db_storage.get(), this->m_created_graph_name, this->m_created_graph_type).detach();

        m_created_graph_name = "";
        m_created_graph_type = "";
    }
}

void
graphquery::interact::CFrameMenuBar::render_create_db() noexcept
{
    if (this->m_is_create_db_opened)
        ImGui::OpenPopup("Create Database");

    ImGui::SetNextWindowSize(ImVec2 {CREATE_WINDOW_WIDTH, CREATE_WINDOW_HEIGHT});
    if (ImGui::BeginPopupModal("Create Database", &m_is_create_db_opened, ImGuiWindowFlags_NoResize))
    {
        render_create_db_info();
        ImGui::EndPopup();
    }
}

void
graphquery::interact::CFrameMenuBar::render_create_db_info() noexcept
{
    if (ImGui::BeginChild("DB Info"))
    {
        ImGui::Text("Complete all fields:");
        ImGui::Separator();

        render_create_db_location();
        render_create_db_name();
        render_create_db_button();
        ImGui::EndChild();
    }
}

void
graphquery::interact::CFrameMenuBar::render_create_db_location() noexcept
{
    static std::string file_path = {};

    ImGui::Text("Location: ");

    if (ImGui::Button("Select path"))
        this->m_db_folder_location_explorer.Open();

    ImGui::SameLine();
    ImGui::Text("%s", file_path.c_str());

    if (this->m_db_folder_location_explorer.HasSelected())
    {
        file_path = this->m_db_folder_location_explorer.GetSelected();

        if (file_path.size() > DB_PATH_SIZE)
        {
            database::_log_system->warning(fmt::format("Select path for DB creation is greater than "
                                                       "specified length ({})",
                                                       DB_PATH_SIZE));
        }
        this->m_created_db_path = file_path;
    }

    this->m_db_folder_location_explorer.Display();
}

void
graphquery::interact::CFrameMenuBar::render_create_db_name() noexcept
{
    ImGui::Text("Name: ");
    ImGui::InputText("##", &this->m_created_db_name, ImGuiInputTextFlags_CharsNoBlank);
}

void
graphquery::interact::CFrameMenuBar::render_create_db_button() noexcept
{
    if (ImGui::Button("Create Database"))
    {
        if (this->m_created_db_name.empty() || this->m_created_db_path.empty())
        {
            database::_log_system->warning("Either the file path or name cannot be empty to create a "
                                           "database");
            return;
        }
        ImGui::CloseCurrentPopup();
        set_create_db_state(false);

        std::thread(&database::storage::CDBStorage::init, database::_db_storage.get(), this->m_created_db_path, this->m_created_db_name).detach();

        this->m_created_db_name.clear();
        this->m_created_db_path.clear();
    }
}

void
graphquery::interact::CFrameMenuBar::render_open_graph() noexcept
{
    if (m_is_open_graph_opened)
        ImGui::OpenPopup("Open Graph");

    if (ImGui::BeginPopupModal("Open Graph", &m_is_open_graph_opened))
    {
        if (database::_db_storage->get_graph_table().empty())
        {
            ImGui::Text("No graph entries");
            ImGui::EndPopup();
            return;
        }

        ImGui::Text("Select a graph to open");
        ImGui::Separator();
        render_open_graph_list();
        render_open_graph_button();

        ImGui::EndPopup();
    }
}

void
graphquery::interact::CFrameMenuBar::render_open_graph_list() noexcept
{
    std::string graphs = {};

    std::for_each(m_graph_table.begin(), m_graph_table.end(), [&graphs](const auto & graph) -> void { graphs += fmt::format("{}{}", graph.second.graph_name, '\0'); });
    graphs += fmt::format("\0");

    ImGui::Combo("##", &m_open_graph_choice, graphs.c_str());
}

void
graphquery::interact::CFrameMenuBar::render_open_graph_button() noexcept
{
    if (ImGui::Button("Open"))
    {
        assert(m_open_graph_choice >= 0 && static_cast<size_t>(m_open_graph_choice) < m_graph_table.size());

        set_open_graph_state(false);
        database::_db_storage->open_graph(std::next(m_graph_table.begin(), m_open_graph_choice)->second.graph_name);
    }
}

void
graphquery::interact::CFrameMenuBar::set_load_db_rollback_state(const bool state) noexcept
{
    this->m_is_load_db_rollback_opened = state;
}

void
graphquery::interact::CFrameMenuBar::render_load_rollback() noexcept
{
    if (this->m_is_load_db_rollback_opened)
        ImGui::OpenPopup("Load Rollback");

    ImGui::SetNextWindowSize(ImVec2 {CREATE_WINDOW_WIDTH, CREATE_WINDOW_HEIGHT});
    if (ImGui::BeginPopupModal("Load Rollback", &this->m_is_load_db_rollback_opened, ImGuiWindowFlags_NoResize))
    {
        if (ImGui::BeginChild("Load Rollback info"))
        {
            ImGui::Text("Select a rollback to load:");
            ImGui::Separator();

            render_load_rollback_entry_data();
            ImGui::EndChild();
        }
        ImGui::EndPopup();
    }
}

void
graphquery::interact::CFrameMenuBar::render_load_rollback_entry_data() noexcept
{
    std::vector<std::string> rollback_table = (*database::_db_graph)->fetch_rollback_table();

    if (rollback_table.empty())
    {
        ImGui::Text("No rollback entries");
        return;
    }

    std::string rollback_entries = {};

    std::for_each(rollback_table.begin(), rollback_table.end(), [&rollback_entries](const std::string & entry_name) -> void { rollback_entries += fmt::format("{}{}", entry_name, '\0'); });
    rollback_entries += fmt::format("\0");

    ImGui::Combo("##", &m_load_rollback_choice, rollback_entries.c_str());

    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    render_load_rollback_button();
}

void
graphquery::interact::CFrameMenuBar::render_load_rollback_button() noexcept
{
    if (ImGui::Button("Load Rollback"))
    {
        ImGui::CloseCurrentPopup();
        set_load_db_rollback_state(false);

        std::thread(&database::storage::ILPGModel::rollback, *database::_db_graph.get(), m_load_rollback_choice).detach();
    }
}

void
graphquery::interact::CFrameMenuBar::set_create_db_state(const bool state) noexcept
{
    this->m_is_create_db_opened = state;
}

void
graphquery::interact::CFrameMenuBar::set_create_graph_state(const bool state) noexcept
{
    this->m_is_create_graph_opened = state;
}

void
graphquery::interact::CFrameMenuBar::set_open_graph_state(const bool state) noexcept
{
    this->m_is_open_graph_opened = state;
}
