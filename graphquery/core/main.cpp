#include "db/system.h"
#include <iostream>

int
main([[maybe_unused]] const int argc, [[maybe_unused]] char * argv[])
{
    if (graphquery::database::initialise(argc, argv) == graphquery::database::EStatus::invalid)
    {
        return EXIT_FAILURE;
    }
    graphquery::database::_log_system->info("GraphQuery has been initialised");

    graphquery::database::_interface->render();
    return EXIT_SUCCESS;
}