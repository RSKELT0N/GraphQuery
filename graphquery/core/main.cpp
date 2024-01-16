#include "db/system.h"

int
main([[maybe_unused]] const int argc, [[maybe_unused]] char * argv[])
{
    if (graphquery::database::initialise(argc, argv) == graphquery::database::EStatus::invalid)
    {
        return EXIT_FAILURE;
    }

    graphquery::database::_log_system->info("GraphQuery has been initialised");

    graphquery::database::_db_storage->init(std::filesystem::path(PROJECT_ROOT) / "../data/", "DB3");
    graphquery::database::_db_storage->open_graph("GR10", "lpg");
    (*graphquery::database::_db_storage->get_graph())->add_vertex(0, "COUNTRY", {{"Population", "10000"}, {"Started", "1890"}});

    graphquery::database::_interface->render();
    return EXIT_SUCCESS;
}