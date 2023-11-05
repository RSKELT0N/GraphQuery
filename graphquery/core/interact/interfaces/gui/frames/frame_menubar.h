#pragma once

#include "interact/interfaces/gui/frame.h"
#include "imfilebrowser.h"

namespace graphquery::interact
{
    class CFrameMenuBar : public IFrame
    {
    public:
        CFrameMenuBar();
        ~CFrameMenuBar() override;

        void Render_Frame() noexcept override;

    private:
        void Render_Save() noexcept;
        void Render_File_Browser() noexcept;

        void SetCreateDBState(bool) noexcept;
        void Render_CreateDB() noexcept;
        void Render_CreateDBInfo() noexcept;
        void Render_CreateDBLocation() noexcept;
        void Render_CreateDBName() noexcept;

        ImGui::FileBrowser m_file_explorer;
        ImGui::FileBrowser m_new_db_location;

        bool m_is_create_db_opened = false;
        static constexpr size_t DB_NAME_SIZE = 20;
        static constexpr size_t CREATE_DB_WINDOW_WIDTH = 500;
        static constexpr size_t CREATE_DB_WINDOW_HEIGHT = 200;
    };



}
