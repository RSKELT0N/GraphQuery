#include "db/system.h"
#include "interact/interfaces/gui/interact_gui.h"

#include <cassert>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    assert(graphquery::database::Initialise() == graphquery::database::EStatus::valid && "Database failed to initialise");
    return graphquery::database::Render();
}