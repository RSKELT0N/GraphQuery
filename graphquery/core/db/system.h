#pragma once

#include <memory>

#include "../log/logger.h"
#include "../log/loggers/log_stdo.h"
#include "../gui/frames/frame_output.h"

namespace graphquery::database
{
    /**********************************************
    ** \brief Status code enumeration to ensure
    **        correctness.
    ***********************************************/
    enum EStatus : uint8_t
    {
        valid = 0,
        invalid = 1
    };

    /**********************************************
    ** \brief Defining the database system to provide
    **        controlling the in-disk file read and
    **        and analytic engine.
    ** @return EStatus status code for init.
    ***********************************************/
    [[nodiscard]] EStatus Initialise() noexcept;

    /**********************************************
    ** \brief In control of initialising the log-system
    **        and adding derived instances for consideration
    **        to render.
    ** @return EStatus status code for init.
    ***********************************************/
    [[nodiscard]] EStatus Initialise_LogSystem() noexcept;

    //~ Internal system functions
    inline static std::shared_ptr<logger::CLogSystem> _log_system;
}