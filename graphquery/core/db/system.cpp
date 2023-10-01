#include "system.h"

[[nodiscard]] graphquery::database::EStatus
graphquery::database::Initialise() noexcept
{
    auto is_valid = static_cast<EStatus>(Initialise_LogSystem() |
                                         Initialise_Interface());
    return is_valid;
}

int graphquery::database::Render() noexcept
{
    (*_interface)->Render();
    return EXIT_SUCCESS;
}

[[nodiscard]] graphquery::database::EStatus
graphquery::database::Initialise_LogSystem() noexcept
{
    _log_system = std::make_unique<logger::CLogSystem>();
    _log_system->Add_Logger(std::make_unique<logger::ILog *>(new logger::CLogSTDO()));
    _log_system->Add_Logger(std::make_unique<logger::ILog *>(new interact::CFrameLog()));
    return graphquery::database::EStatus::valid;
}

[[nodiscard]] graphquery::database::EStatus
graphquery::database::Initialise_Interface() noexcept
{
    _interface = std::make_unique<interact::IInteract *>(new interact::CInteractGUI());
    return graphquery::database::EStatus::valid;
}











