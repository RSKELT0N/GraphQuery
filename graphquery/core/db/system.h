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
    [[nodiscard]] EStatus Initialise() noexcept;

    /**********************************************
    ** \brief Rendering the interface of the DB.
    ** @return EStatus status code for init.
    ***********************************************/
    [[nodiscard]] int Render() noexcept;

    /**********************************************
    ** \brief In control of initialising the log-system
    **        and adding derived instances for consideration
    **        to render.
    ** @return EStatus status code for init.
    ***********************************************/
    [[nodiscard]] EStatus Initialise_LogSystem() noexcept;

    /**********************************************
    ** \brief In control of initialising the interface
    **        to provide a way of viewing/controlling the db.
    ** @return EStatus status code for init.
    ***********************************************/
    [[nodiscard]] EStatus Initialise_Interface() noexcept;

    //~ Internal system functions
    inline static std::unique_ptr<interact::IInteract *> _interface;
    inline static std::unique_ptr<logger::CLogSystem> _log_system;
}