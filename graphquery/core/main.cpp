#include <cstdlib>
#include <gui/gui.hpp>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    if(graphquery::gui::Initialise("Graph Query") != 0) return 1;
    graphquery::gui::Render();

    return EXIT_SUCCESS;
}