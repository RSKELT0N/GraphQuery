#include "system.h"

#include <csignal>

#include "log/loggers/log_stdo.h"

namespace
{
    graphquery::database::EStatus Initialise_Logging() noexcept
    {
        graphquery::database::_log_system->add_logger(std::make_shared<graphquery::logger::CLogSTDO>());

        return graphquery::database::EStatus::valid;
    }
} // namespace

namespace graphquery::database
{
    //~ Linked symbol to the existing db bool
    bool _existing_db_loaded = false;
    //~ Linked symbol of the log system.
    std::shared_ptr<logger::CLogSystem> _log_system = logger::CLogSystem::GetInstance();
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
                   exit(0);
               });

        EStatus status = Initialise_Logging();
        return status;
    }
} // namespace graphquery::database
