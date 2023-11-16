#include "db/system.h"
#include "db/storage/diskdriver.h"

#include "dylib.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    if (graphquery::database::Initialise(argc, argv) == graphquery::database::EStatus::invalid)
    {
        return EXIT_FAILURE;
    }

    graphquery::database::_interface->Render();
    return EXIT_SUCCESS;
}