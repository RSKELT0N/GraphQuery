#include "system.h"

#include "../log/loggers/log_stdo.h"

namespace
{
    void Initialise_Logging() noexcept
    {
        graphquery::database::_log_system->Add_Logger(std::make_shared<graphquery::logger::CLogSTDO>());
    }
}

graphquery::database::EStatus
graphquery::database::Initialise([[maybe_unused]] int argc,
                                 [[maybe_unused]] char ** argv) noexcept
{
    Initialise_Logging();
    return EStatus::valid;
}












