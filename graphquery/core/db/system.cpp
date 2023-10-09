#include "system.h"

#include "log/loggers/log_stdo.h"
#include "interact/interfaces/gui/interact_gui.h"

namespace
{
    void Initialise_Logging() noexcept
    {
        graphquery::database::_log_system->Add_Logger(std::make_shared<graphquery::logger::CLogSTDO>());
    }
}

namespace graphquery::database
{
    //~ Linked symbol of the log system.
    std::unique_ptr<logger::CLogSystem> _log_system = std::make_unique<logger::CLogSystem>();
    //~ Linked symbol of the interface towards the database.
    std::unique_ptr<interact::IInteract> _interface = std::make_unique<interact::CInteractGUI>();

    graphquery::database::EStatus Initialise([[maybe_unused]] int argc, [[maybe_unused]] char **argv) noexcept
    {
        Initialise_Logging();
        return EStatus::valid;
    }
}












