#pragma once

#include "lib.h"
#include "../log/logger.h"
#include "../interact/interact.h"
#include "../log/loggers/log_stdo.h"
#include "../interact/interfaces/gui/interact_gui.h"
#include "../interact/interfaces/gui/frames/frame_output.h"

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
    [[nodiscard("Ensure the initialise status is checked.")]]
    EStatus Initialise([[maybe_unused]] int argc, [[maybe_unused]] char ** argv) noexcept;

    //~ Log system instance for rendering output.
    const static std::unique_ptr<logger::CLogSystem> _log_system = std::make_unique<graphquery::logger::CLogSystem>();
    //~ Interface instance for providing access to the database.
    const static std::unique_ptr<interact::IInteract> _interface = std::make_unique<graphquery::interact::CInteractGUI>();
}