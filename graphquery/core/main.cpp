#include "db/system.h"
#include "gui/gui.h"

#include <cassert>

using namespace graphquery;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    assert(database::Initialise() == database::valid && "Database failed to initialise");
    assert(gui::Initialise("Graph Query") == 0 && "GUI failed to initialise");

    graphquery::gui::Render();
    return EXIT_SUCCESS;
}