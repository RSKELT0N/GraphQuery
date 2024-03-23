#include "system.h"

#include "log/loggers/log_stdo.h"
#include "storage/config.h"

#include <csignal>
#include <thread>

namespace
{
    bool _sync;
    void setup_seg_handler()
    {
        signal(SIGINT | SIGTERM,
               [](const int param) -> void
               {
                   graphquery::database::_db_storage->close();
                   graphquery::database::_interface->clean_up();
                   exit(param);
               });
    }

    graphquery::database::EStatus Initialise_Logging() noexcept
    {
        graphquery::database::_log_system->add_logger(std::make_shared<graphquery::logger::CLogSTDO>());

        return graphquery::database::EStatus::valid;
    }

    void heartbeat() noexcept
    {
        while (true)
        {
            if (graphquery::database::_db_storage->get_is_graph_loaded() && _sync)
                (*graphquery::database::_db_graph)->sync_graph();

            std::this_thread::sleep_for(graphquery::database::storage::CFG_SYSTEM_HEARTBEAT_INTERVAL);
        }
    }
} // namespace

namespace graphquery::database
{
    //~ Linked symbol of the log system.
    std::shared_ptr<logger::CLogSystem> _log_system = logger::CLogSystem::get_instance();
    //~ Linked symbol of the interface towards the database.
    std::unique_ptr<interact::IInteract> _interface = std::make_unique<interact::CInteractGUI>();
    //~ Linked symbol of the db storage.
    std::unique_ptr<storage::CDBStorage> _db_storage = std::make_unique<storage::CDBStorage>();
    //~ Linked symbol of the analytic engine.
    std::unique_ptr<analytic::CAnalyticEngine> _db_analytic = std::make_unique<analytic::CAnalyticEngine>(_db_storage->get_graph());
    //~ Linked symbol of the query engine.
    std::unique_ptr<query::CQueryEngine> _db_query = std::make_unique<query::CQueryEngine>(_db_storage->get_graph());
    //~ Linked symbol of the db loaded graph.
    std::shared_ptr<storage::ILPGModel *> _db_graph = _db_storage->get_graph();

    EStatus initialise([[maybe_unused]] int argc, [[maybe_unused]] char ** argv) noexcept
    {
        setup_seg_handler();
        const EStatus status = Initialise_Logging();

        _enable_sync_();
        std::thread(&heartbeat).detach();
        return status;
    }

    void _enable_sync_() noexcept
    {
        _sync = true;
        _log_system->info("Synchronisation has been enabled");
    }

    void _disable_sync_() noexcept
    {
        _sync = false;
        _log_system->info("Synchronisation has been disabled");
    }

    const bool & _get_sync_state_() noexcept
    {
        return _sync;
    }
} // namespace graphquery::database
