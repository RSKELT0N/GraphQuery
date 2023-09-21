#include <log/loggers/log_gui.hpp>
#include <imgui.h>

graphquery::logger::CLogGUI::~CLogGUI() {
    ILog::~ILog();
}

void
graphquery::logger::CLogGUI::Log(graphquery::logger::ELogType log_type, const std::string& out) const noexcept
{
    std::string ret;

    switch(log_type)
    {
        case ELogType::debug:
            ret += fmt::format("[{}]", fmt::format(fg(fmt::color::cyan), "DEBUG"));
            break;
        case ELogType::info:
            ret += fmt::format("[{}]", fmt::format(fg(fmt::color::green), "INFO"));
            break;
        case ELogType::warning:
            ret += fmt::format("[{}]", fmt::format(fg(fmt::color::yellow), "WARNING"));
            break;
        case ELogType::error:
            ret += fmt::format("[{}]", fmt::format(fg(fmt::color::red), "ERROR"));
            break;
    }

    if(ImGui::Begin("Output")) {}
    ImGui::Text("%s %s", ret.c_str(), out.c_str());
    ImGui::End();
}