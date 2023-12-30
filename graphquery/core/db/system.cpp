#include "system.h"

#include "log/loggers/log_stdo.h"
#include "storage/config.h"

#include <csignal>
#include <thread>
#include <chrono>

namespace
{
    graphquery::database::EStatus Initialise_Logging() noexcept
    {
        graphquery::database::_log_system->add_logger(std::make_shared<graphquery::logger::CLogSTDO>());

        return graphquery::database::EStatus::valid;
    }

    void heartbeat() noexcept
    {
        while (true)
        {
            if (graphquery::database::_db_storage->get_is_db_loaded() && graphquery::database::_db_storage->get_is_graph_loaded())
            {
                graphquery::database::_db_storage->get_graph()->save_graph();
                graphquery::database::_log_system->debug("Graph has been saved");
            }

            graphquery::database::_log_system->debug(fmt::format("System heartbeat sleeping for {} seconds..", graphquery::database::storage::SYSTEM_HEARTBEAT_INTERVAL.count()));
            std::this_thread::sleep_for(graphquery::database::storage::SYSTEM_HEARTBEAT_INTERVAL);
        }
    }
} // namespace

namespace graphquery::database
{
    //~ Linked symbol to the existing db bool
    bool _existing_db_loaded = false;
    //~ Linked symbol of the log system.
    std::shared_ptr<logger::CLogSystem> _log_system = logger::CLogSystem::get_instance();
    //~ Linked symbol of the interface towards the database.
    std::unique_ptr<interact::IInteract> _interface = std::make_unique<interact::CInteractGUI>();
    //~ Linked symbol of the interface towards the database.
    std::unique_ptr<storage::CDBStorage> _db_storage = std::make_unique<storage::CDBStorage>();

    EStatus initialise([[maybe_unused]] int argc, [[maybe_unused]] char ** argv) noexcept
    {
        signal(SIGINT | SIGTERM,
               [](int param) -> void
               {
                   _db_storage->close();
                   _interface->clean_up();
               });

        EStatus status = Initialise_Logging();
        std::thread(heartbeat).detach();
        return status;
    }
} // namespace graphquery::database
