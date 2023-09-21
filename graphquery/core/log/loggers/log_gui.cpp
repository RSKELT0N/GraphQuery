#include <log/loggers/log_gui.hpp>

#include <imgui.h>

graphquery::logger::CLogGUI::~CLogGUI() {
    ILog::~ILog();
}

void
graphquery::logger::CLogGUI::Log(graphquery::logger::ELogType log_type, const std::string& out) const noexcept
{
    ImVec4 col;

    if(ImGui::Begin("Output")) {}
    ImGui::Text("[");
    ImGui::SameLine();

    switch(log_type)
    {
        case ELogType::debug:
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 255.0f), "DEBUG");       // cyan
            break;
        case ELogType::info:
            ImGui::TextColored(ImVec4(0.0f, 128.0f, 0.0f, 255.0f), "%s", "INFO");     // green
            break;
        case ELogType::warning:
            ImGui::TextColored(ImVec4(255.0f, 255.0f, 0.0f, 255.0f), "WARNING");   // yellow
            break;
        case ELogType::error:
            ImGui::TextColored(ImVec4(255.0f, 0.0f, 0.0f, 255.0f), "ERROR");     // red
            break;
    }

    ImGui::SameLine();
    ImGui::Text("%s %s", "]", out.c_str());
    ImGui::End();
}