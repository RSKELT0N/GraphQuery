#include "frame_menubar.h"

#include "db/system.h"
#include "fmt/format.h"

#include "imgui_stdlib.h"

graphquery::interact::CFrameMenuBar::~CFrameMenuBar() = default;

<<<<<<< HEAD
graphquery::interact::CFrameMenuBar::CFrameMenuBar(const bool & is_db_loaded) : m_is_db_loaded(is_db_loaded)
=======
graphquery::interact::CFrameMenuBar::CFrameMenuBar(const bool & is_db_loaded, const std::vector<database::storage::CDBStorage::SGraph_Entry_t> & graph_table) : m_is_db_loaded(is_db_loaded), m_graph_table(graph_table)
>>>>>>> 4158259 (Add graph table.)
{
    this->m_db_master_file_explorer = ImGui::FileBrowser(ImGuiFileBrowserFlags_CloseOnEsc |
                                                         ImGuiFileBrowserFlags_CreateNewDir);

    this->m_db_master_file_explorer.SetTitle("Open Database");
    this->m_db_master_file_explorer.SetTypeFilters({".gdb"});

    this->m_db_folder_location_explorer = ImGui::FileBrowser(ImGuiFileBrowserFlags_CloseOnEsc   |
                                                             ImGuiFileBrowserFlags_CreateNewDir |
                                                             ImGuiFileBrowserFlags_SelectDirectory);

    this->m_db_folder_location_explorer.SetTitle("Select Location Path");
}

void
graphquery::interact::CFrameMenuBar::Render_Frame() noexcept
{
    if (ImGui::BeginMainMenuBar())
    {
        Render_CreateMenu();
        Render_OpenMenu();
        Render_DBMenu();
        ImGui::EndMainMenuBar();
    }

    Render_CreateDB();
    Render_CreateGraph();
    Render_OpenDB();
    Render_OpenGraph();
}

void
graphquery::interact::CFrameMenuBar::Render_DBMenu() noexcept
{
    if(m_is_db_loaded && ImGui::BeginMenu("Database"))
    {
        if(ImGui::MenuItem("Close"))
            database::_db_storage->Close();

        ImGui::EndMenu();
    }
}

void
graphquery::interact::CFrameMenuBar::Render_CreateMenu() noexcept
{
    if (ImGui::BeginMenu("Create"))
    {
        if (ImGui::MenuItem("Database"))
            SetCreateDBState(true);

        if(m_is_db_loaded && ImGui::MenuItem("Graph"))
            SetCreateGraphState(true);

        ImGui::EndMenu();
    }
}

void
graphquery::interact::CFrameMenuBar::Render_OpenMenu() noexcept
{
    if (ImGui::BeginMenu("Open", "Ctrl+o"))
    {
        if(ImGui::MenuItem("Database"))
            this->m_db_master_file_explorer.Open();

        if(m_is_db_loaded && ImGui::MenuItem("Graph"))
<<<<<<< HEAD
            this->m_db_master_file_explorer.Open();
=======
            SetOpenGraphState(true);
>>>>>>> 4158259 (Add graph table.)

        ImGui::EndMenu();
    }
}

void
graphquery::interact::CFrameMenuBar::Render_OpenDB() noexcept
{
    m_db_master_file_explorer.Display();

    if(m_db_master_file_explorer.HasSelected())
    {
        const std::filesystem::path db_master_file_path = m_db_master_file_explorer.GetSelected();
        database::_db_storage->Init(db_master_file_path.generic_string());
        m_db_master_file_explorer.ClearSelected();
    }
}

void
graphquery::interact::CFrameMenuBar::Render_CreateGraph() noexcept
{
    if(this->m_is_create_graph_opened)
        ImGui::OpenPopup("Create Graph");

    ImGui::SetNextWindowSize(ImVec2{CREATE_WINDOW_WIDTH, CREATE_WINDOW_HEIGHT});
    if (ImGui::BeginPopupModal("Create Graph", &this->m_is_create_graph_opened, ImGuiWindowFlags_NoResize))
    {
        Render_CreateGraphInfo();
        ImGui::EndPopup();
    }
}

void
graphquery::interact::CFrameMenuBar::Render_CreateGraphInfo() noexcept
{
    if(ImGui::BeginChild("Graph Info"))
    {
        ImGui::Text("Complete all fields:");
        ImGui::Separator();

        Render_CreateGraphName();
        Render_CreateGraphType();
        Render_CreateGraphButton();
    }
    ImGui::EndChild();
}

void
graphquery::interact::CFrameMenuBar::Render_CreateGraphName() noexcept
{
    ImGui::Text("Name: ");
    ImGui::InputText("##", &this->m_created_graph_name, ImGuiInputTextFlags_CharsNoBlank);
}

void
graphquery::interact::CFrameMenuBar::Render_CreateGraphType() noexcept
{
    ImGui::Text("Model Type (shared library): ");
    ImGui::InputText("###", &this->m_created_graph_type, ImGuiInputTextFlags_CharsNoBlank);
}

void
graphquery::interact::CFrameMenuBar::Render_CreateGraphButton() noexcept
{
    if(ImGui::Button("Create Graph"))
    {
        if((m_created_graph_name.empty() || m_created_graph_name.length() > database::storage::GRAPH_NAME_LENGTH)
            || (m_created_graph_type.empty() || m_created_graph_type.length() > database::storage::GRAPH_MODEL_TYPE_LENGTH))
        {
            database::_log_system->Warning(fmt::format("Either the name or type cannot be empty or larger than configured size (Name: {}, Type: {}) to create a graph",
                                                         database::storage::GRAPH_NAME_LENGTH,
                                                         database::storage::GRAPH_MODEL_TYPE_LENGTH));
            return;
        }
        database::_db_storage->CreateGraph(this->m_created_graph_name, this->m_created_graph_type);
        m_created_graph_name = "";
        m_created_graph_type = "";
        SetCreateGraphState(false);
    }
}

void
graphquery::interact::CFrameMenuBar::Render_CreateDB() noexcept
{
    if(this->m_is_create_db_opened)
        ImGui::OpenPopup("Create Database");

    ImGui::SetNextWindowSize(ImVec2{CREATE_WINDOW_WIDTH, CREATE_WINDOW_HEIGHT});
    if(ImGui::BeginPopupModal("Create Database", &m_is_create_db_opened, ImGuiWindowFlags_NoResize))
    {
        Render_CreateDBInfo();
        ImGui::EndPopup();
    }
}

void
graphquery::interact::CFrameMenuBar::Render_CreateDBInfo() noexcept
{
    if(ImGui::BeginChild("DB Info"))
    {
        ImGui::Text("Complete all fields:");
        ImGui::Separator();

        Render_CreateDBLocation();
        Render_CreateDBName();
        Render_CreateDBButton();
    }
    ImGui::EndChild();
}

void
graphquery::interact::CFrameMenuBar::Render_CreateDBLocation() noexcept
{
    static std::string file_path = {};

    ImGui::Text("Location: ");

    if(ImGui::Button("Select path"))
        this->m_db_folder_location_explorer.Open();

    ImGui::SameLine();
    ImGui::Text("%s", file_path.c_str());

    if(this->m_db_folder_location_explorer.HasSelected())
    {
        file_path = this->m_db_folder_location_explorer.GetSelected().string();

        if(file_path.size() > DB_PATH_SIZE)
        {
            database::_log_system->Warning(fmt::format("Select path for DB creation is greater than specified length ({})", DB_PATH_SIZE));
        }
        this->m_created_db_path = file_path;
    }

    this->m_db_folder_location_explorer.Display();
}

void
graphquery::interact::CFrameMenuBar::Render_CreateDBName() noexcept
{
    ImGui::Text("Name: ");
    ImGui::InputText("##", &this->m_created_db_name, ImGuiInputTextFlags_CharsNoBlank);
    ImGui::SameLine();
    ImGui::Text(".gdb");
}

void
graphquery::interact::CFrameMenuBar::Render_CreateDBButton() noexcept
{
    if(ImGui::Button("Create Database"))
    {
        if(this->m_created_db_name.empty() || this->m_created_db_path.empty())
        {
            database::_log_system->Warning("Either the file path or name cannot be empty to create a database");
            return;
        }
        database::_db_storage->Init(fmt::format("{}/{}.gdb", this->m_created_db_path, this->m_created_db_name));
        this->m_created_db_name = "";
        this->m_created_db_path = "";
        SetCreateDBState(false);
    }
}

void
graphquery::interact::CFrameMenuBar::Render_OpenGraph() noexcept
{
    if(m_is_open_graph_opened)
        ImGui::OpenPopup("Open Graph");

    if (ImGui::BeginPopupModal("Open Graph", &m_is_open_graph_opened))
    {
        ImGui::Text("Select a graph to open");
        ImGui::Separator();
        Render_OpenGraphList();
        Render_OpenGraphButton();

        ImGui::EndPopup();
    }
}

void
graphquery::interact::CFrameMenuBar::Render_OpenGraphList() noexcept
{
    std::string graphs = {};

    std::for_each(m_graph_table.begin(), m_graph_table.end(), [&graphs](const auto & graph) -> void
    {
        graphs += fmt::format("{}{}", graph.graph_name, '\0');
    });
    graphs += fmt::format("\0");

    ImGui::Combo("##", &m_open_graph_choice, graphs.c_str());
}

void
graphquery::interact::CFrameMenuBar::Render_OpenGraphButton() noexcept
{
    if (ImGui::Button("Open"))
    {
        assert(m_open_graph_choice >= 0 && m_open_graph_choice < m_graph_table.size());

        database::_db_storage->OpenGraph(m_graph_table.at(m_open_graph_choice).graph_name, m_graph_table.at(m_open_graph_choice).graph_type);

        ImGui::CloseCurrentPopup();
        SetOpenGraphState(false);
    }
}

void
graphquery::interact::CFrameMenuBar::SetCreateDBState(const bool state) noexcept
{
    this->m_is_create_db_opened = state;
}

void
graphquery::interact::CFrameMenuBar::SetCreateGraphState(const bool state) noexcept
{
    this->m_is_create_graph_opened = state;
}

void
graphquery::interact::CFrameMenuBar::SetOpenGraphState(const bool state) noexcept
{
    this->m_is_open_graph_opened = state;
}
