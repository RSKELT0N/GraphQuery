#include "system.h"

#include "log/loggers/log_stdo.h"
#include "storage/config.h"

#include <csignal>
#include <thread>

namespace
{
    graphquery::database::EStatus Initialise_Logging() noexcept
    {
        graphquery::database::_log_system->add_logger(std::make_shared<graphquery::logger::CLogSTDO>());

        return graphquery::database::EStatus::valid;
    }

    void heartbeat() noexcept
    {
        while (false)
        {
            if (graphquery::database::_db_storage->get_is_graph_loaded())
                (*graphquery::database::_db_storage->get_graph())->save_graph();

            std::this_thread::sleep_for(graphquery::database::storage::CFG_SYSTEM_HEARTBEAT_INTERVAL);
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
    //~ Linked symbol of the db storage.
    std::unique_ptr<storage::CDBStorage> _db_storage = std::make_unique<storage::CDBStorage>();
    //~ Linked symbol of the analytic engine.
    std::unique_ptr<analytic::CAnalyticEngine> _db_analytic = std::make_unique<analytic::CAnalyticEngine>(_db_storage->get_graph());

    EStatus initialise([[maybe_unused]] int argc, [[maybe_unused]] char ** argv) noexcept
    {
        signal(SIGINT | SIGTERM,
               [](const int param) -> void
               {
                   _db_storage->close();
                   _interface->clean_up();
                   exit(param);
               });

        const EStatus status = Initialise_Logging();

        std::thread(&heartbeat).detach();
        return status;
    }
} // namespace graphquery::database
