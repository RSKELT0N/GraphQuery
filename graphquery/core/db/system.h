#pragma once

#include "db/utils/lib.h"
#include "../log/logsystem.h"
#include "../interact/interact.h"

#include <memory>

namespace graphquery::database
{
    /**********************************************
    ** \brief Status code enumeration to ensure
    **        correctness.
    ***********************************************/
    enum class EStatus : uint8_t
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
    [[nodiscard]]
    EStatus Initialise([[maybe_unused]] int argc, [[maybe_unused]] char ** argv) noexcept;

    //~ Log system instance for rendering output.
    extern std::unique_ptr<logger::CLogSystem> _log_system;
    //~ Interface instance for providing access to the database.
    extern std::unique_ptr<interact::IInteract> _interface;
}