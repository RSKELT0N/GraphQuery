#include "db/system.h"

#ifndef NDEBUG
#define DB_NAME    "DB0"
#define GRAPH_NAME "GR0"
#endif

int
main([[maybe_unused]] const int argc, [[maybe_unused]] char * argv[])
{
    if (graphquery::database::initialise(argc, argv) == graphquery::database::EStatus::invalid)
        return EXIT_FAILURE;

    graphquery::database::_log_system->info("GraphQuery has been initialised");

#ifndef NDEBUG
    graphquery::database::_db_storage->init(std::filesystem::current_path(), DB_NAME);

    if (graphquery::database::_db_storage->check_if_graph_exists(GRAPH_NAME))
        graphquery::database::_db_storage->open_graph(GRAPH_NAME);
    else
        graphquery::database::_db_storage->create_graph(GRAPH_NAME, "lpg_mmap");
#endif

    graphquery::database::_interface->render();
    return EXIT_SUCCESS;
}