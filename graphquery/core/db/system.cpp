#include "system.h"

[[nodiscard]] graphquery::database::EStatus graphquery::database::Initialise_LogSystem() noexcept
{
    _log_system = std::make_shared<logger::CLogSystem>();
    _log_system->Add_Logger(std::make_unique<logger::ILog *>(new logger::CLogSTDO()));
    _log_system->Add_Logger(std::make_unique<logger::ILog *>(new gui::CFrameLog()));
    return graphquery::database::EStatus::valid;
}

[[nodiscard]] graphquery::database::EStatus graphquery::database::Initialise() noexcept
{
    EStatus is_valid = Initialise_LogSystem();
    return is_valid;
}







