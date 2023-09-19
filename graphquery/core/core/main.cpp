#include <cstdlib>
#include <gui.hpp>
#include <logger.hpp>
#include <log_stdo.hpp>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    graphquery::logger::CLogSystem logSystem;
    logSystem.Add_Logger(new graphquery::logger::CLogSTDO());

    if(graphquery::gui::Initialise("Graph Query") != 0) return 1;
    graphquery::gui::Render(); return EXIT_SUCCESS;
}