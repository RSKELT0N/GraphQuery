#include <cstdlib>
#include <gui/gui.hpp>
#include <log/logger.hpp>
#include <log/loggers/log_stdo.hpp>
#include <log/loggers/log_gui.hpp>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    graphquery::logger::CLogSystem logSystem;
    logSystem.Add_Logger(new graphquery::logger::CLogSTDO());
    logSystem.Add_Logger(new graphquery::logger::CLogGUI());
    logSystem.Log(graphquery::logger::ELogType::info, "Hello World!");

    if(graphquery::gui::Initialise("Graph Query") != 0) return 1;
    graphquery::gui::Render();

    return EXIT_SUCCESS;
}