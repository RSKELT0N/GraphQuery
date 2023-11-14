#include "frame_menubar.h"

#include "db/system.h"
#include "fmt/format.h"

#include "imgui_stdlib.h"

graphquery::interact::CFrameMenuBar::~CFrameMenuBar() = default;

graphquery::interact::CFrameMenuBar::CFrameMenuBar()
{
    this->m_db_master_file_explorer = ImGui::FileBrowser(ImGuiFileBrowserFlags_CloseOnEsc
                                               | ImGuiFileBrowserFlags_CreateNewDir);

    this->m_db_master_file_explorer.SetTitle("Open Database");
    this->m_db_master_file_explorer.SetTypeFilters({".gdb"});

    this->m_db_folder_location_explorer = ImGui::FileBrowser(ImGuiFileBrowserFlags_CloseOnEsc
                                                | ImGuiFileBrowserFlags_CreateNewDir
                                                | ImGuiFileBrowserFlags_SelectDirectory);

    this->m_db_folder_location_explorer.SetTitle("Select Location Path");
}

void graphquery::interact::CFrameMenuBar::Render_Frame() noexcept
{
    if (ImGui::BeginMainMenuBar())
    {
        Render_CreateMenu();
        Render_OpenMenu();
        ImGui::EndMainMenuBar();
    }

    Render_File_Browser();
    Render_CreateDB();
}

void graphquery::interact::CFrameMenuBar::Render_CreateMenu() noexcept
{
    if (ImGui::BeginMenu("Create"))
    {
        if (ImGui::MenuItem("Database"))
            SetCreateDBState(true);

        if(graphquery::database::_storage->IsExistingDBLoaded() && ImGui::MenuItem("Graph"))
            SetCreateGraphState(true);

        ImGui::EndMenu();
    }
}

void graphquery::interact::CFrameMenuBar::Render_OpenMenu() noexcept
{
    if (ImGui::BeginMenu("Open", "Ctrl+o"))
    {
        if(ImGui::MenuItem("Database"))
            this->m_db_master_file_explorer.Open();

        if(graphquery::database::_storage->IsExistingDBLoaded() && ImGui::MenuItem("Graph"))
            this->m_db_master_file_explorer.Open();

        ImGui::EndMenu();
    }
}

void graphquery::interact::CFrameMenuBar::Render_File_Browser() noexcept
{
    m_db_master_file_explorer.Display();

    if(m_db_master_file_explorer.HasSelected())
    {
        const std::filesystem::path db_master_file_path = m_db_master_file_explorer.GetSelected();
        graphquery::database::_storage->Init(db_master_file_path.generic_string());
        m_db_master_file_explorer.ClearSelected();
    }
}

void graphquery::interact::CFrameMenuBar::Render_CreateDB() noexcept
{
    if(this->m_is_create_db_opened)
    {
        ImGui::SetNextWindowSize(ImVec2{CREATE_DB_WINDOW_WIDTH, CREATE_DB_WINDOW_HEIGHT});
        if (ImGui::Begin("Create Database", &this->m_is_create_db_opened, ImGuiWindowFlags_NoResize |
                                                                          ImGuiWindowFlags_NoMove   |
                                                                          ImGuiWindowFlags_NoScrollbar))
        {
            Render_CreateDBInfo();
        }
        ImGui::End();
    }
}

void graphquery::interact::CFrameMenuBar::Render_CreateDBInfo() noexcept
{
    if(ImGui::BeginChild("DB Info", ImVec2{CREATE_DB_WINDOW_WIDTH, CREATE_DB_WINDOW_HEIGHT}))
    {
        ImGui::Text("Complete all fields:");
        ImGui::Separator();

        Render_CreateDBLocation();
        Render_CreateDBName();
        Render_CreateDBButton();
    }
    ImGui::EndChild();
}

void graphquery::interact::CFrameMenuBar::Render_CreateDBLocation() noexcept
{
    static std::string file_path;

    ImGui::Text("Location: ");
    ImGui::SameLine();
    ImGui::Text("%s", file_path.c_str());
    ImGui::SameLine();

    if(ImGui::Button("Select path"))
    {
        this->m_db_folder_location_explorer.Open();
    }

    if(this->m_db_folder_location_explorer.HasSelected())
    {
        file_path = this->m_db_folder_location_explorer.GetSelected().string();

        if(file_path.size() > DB_PATH_SIZE)
        {
            graphquery::database::_log_system->Warning(fmt::format("Select path for DB creation is greater than specified length ({})", DB_PATH_SIZE));
        }
        this->m_created_db_path = file_path;
    }

    this->m_db_folder_location_explorer.Display();
}

void graphquery::interact::CFrameMenuBar::Render_CreateDBName() noexcept
{
    ImGui::Text("Name: ");
    ImGui::SameLine();
    ImGui::InputText("##", &this->m_created_db_name, ImGuiInputTextFlags_CharsNoBlank);
    ImGui::SameLine();
    ImGui::Text(".gdb");
}

void graphquery::interact::CFrameMenuBar::Render_CreateDBButton() noexcept
{
    if(ImGui::Button("Create"))
    {
        if(this->m_created_db_name.empty() || this->m_created_db_path.empty())
        {
            graphquery::database::_log_system->Warning("Either the file path or name cannot be empty to create a database");
            return;
        }
        graphquery::database::_storage->Init(fmt::format("{}/{}.gdb", this->m_created_db_path, this->m_created_db_name));
        this->m_created_db_name = "";
        this->m_created_db_path = "";
        SetCreateDBState(false);
    }}

void
graphquery::interact::CFrameMenuBar::SetCreateDBState(bool state) noexcept
{
    this->m_is_create_db_opened = state;
}

void
graphquery::interact::CFrameMenuBar::SetCreateGraphState(bool state) noexcept
{
    this->m_is_create_graph_opened = state;
}
