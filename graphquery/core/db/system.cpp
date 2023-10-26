#include "system.h"

#include "log/loggers/log_stdo.h"
#include "interact/interfaces/gui/interact_gui.h"

namespace
{
    graphquery::database::EStatus Initialise_Logging() noexcept
    {
        graphquery::database::_log_system->Add_Logger(std::make_shared<graphquery::logger::CLogSTDO>());

        return graphquery::database::EStatus::valid;
    }
}

namespace graphquery::database
{
    //~ Linked symbol of the log system.
    std::unique_ptr<logger::CLogSystem> _log_system = std::make_unique<logger::CLogSystem>();
    //~ Linked symbol of the interface towards the database.
    std::unique_ptr<interact::IInteract> _interface = std::make_unique<interact::CInteractGUI>();
    //~ Linked symbol of the interface towards the database.
    std::unique_ptr<storage::CStorage> _storage = std::make_unique<storage::CStorage>();

    graphquery::database::EStatus Initialise([[maybe_unused]] int argc, [[maybe_unused]] char **argv) noexcept
    {
        EStatus status = Initialise_Logging();
        return status;
    }
}












