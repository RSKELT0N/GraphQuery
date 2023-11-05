#include "frame_menubar.h"

graphquery::interact::CFrameMenuBar::~CFrameMenuBar() = default;

graphquery::interact::CFrameMenuBar::CFrameMenuBar()
{
    this->m_file_explorer = ImGui::FileBrowser(ImGuiFileBrowserFlags_CloseOnEsc
                                               | ImGuiFileBrowserFlags_CreateNewDir);

    this->m_file_explorer.SetTitle("Open Database");
    this->m_file_explorer.SetTypeFilters({".plg"});

    this->m_new_db_location = ImGui::FileBrowser(ImGuiFileBrowserFlags_CloseOnEsc
                                                | ImGuiFileBrowserFlags_CreateNewDir
                                                | ImGuiFileBrowserFlags_SelectDirectory);

    this->m_file_explorer.SetTitle("Select Location Path");
}

void graphquery::interact::CFrameMenuBar::Render_Frame() noexcept
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Create"))
            {
                SetCreateDBState(true);
            }
            if (ImGui::MenuItem("Open", "Ctrl+o"))
            {
                this->m_file_explorer.Open();
            }
            if (ImGui::MenuItem("Save", "Ctrl+S"))
            {
                Render_Save();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    Render_File_Browser();
    Render_CreateDB();
}

void graphquery::interact::CFrameMenuBar::SetCreateDBState(bool state) noexcept
{
    this->m_is_create_db_opened = state;
}

void graphquery::interact::CFrameMenuBar::Render_File_Browser() noexcept
{
    this->m_file_explorer.Display();

    if(this->m_file_explorer.HasSelected())
    {
        this->m_file_explorer.ClearSelected();
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

        if(ImGui::Button("Create"))
        {
            SetCreateDBState(false);
        }
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
        this->m_new_db_location.Open();
    }

    if(this->m_new_db_location.HasSelected())
    {
        file_path = this->m_new_db_location.GetSelected().string();
    }

    this->m_new_db_location.Display();
}

void graphquery::interact::CFrameMenuBar::Render_CreateDBName() noexcept
{
    static char db_name[DB_NAME_SIZE];
    ImGui::Text("Name: ");
    ImGui::SameLine();
    ImGui::InputText("##", db_name, DB_NAME_SIZE);
}

void graphquery::interact::CFrameMenuBar::Render_Save() noexcept {}
