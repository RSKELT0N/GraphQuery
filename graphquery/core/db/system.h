/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file system.h
 * \brief System header file containing the necessary
 *        functionality for the graph db. Central point
 *        to access the core instances.
 ************************************************************/

#pragma once

#include "db/storage/dbstorage.h"
#include "interact/interfaces/gui/interact_gui.h"
#include "log/logsystem/logsystem.h"
#include "db/analytic/analytic.h"
#include "db/query/query.h"

#include <cstdint>

namespace graphquery::database
{
    /**********************************************
    ** \brief Status code enumeration to ensure
    **        correctness.
    ***********************************************/
    enum class EStatus : uint8_t
    {
        valid   = 0,
        invalid = 1
    };

    /**********************************************
    ** \brief Defining the database system to provide
    **        controlling the in-disk file read and
    **        and analytic engine.
    ** @return EStatus status code for init.
    ***********************************************/
    [[nodiscard]] EStatus initialise([[maybe_unused]] int argc, [[maybe_unused]] char ** argv) noexcept;
    void enable_sync() noexcept;
    void disable_sync() noexcept;

    //~ Log system instance for rendering output.
    extern std::shared_ptr<logger::CLogSystem> _log_system;
    //~ Interface instance for providing access to the database.
    extern std::unique_ptr<interact::IInteract> _interface;
    //~ Instance of the graph data model.
    extern std::unique_ptr<storage::CDBStorage> _db_storage;
    //~ Instance of the graph anayltic engine.
    extern std::unique_ptr<analytic::CAnalyticEngine> _db_analytic;
    //~ Instance of the graph query engine.
    extern std::unique_ptr<query::CQueryEngine> _db_query;
    //~ Instance of the currently loaded graph
    extern std::shared_ptr<storage::ILPGModel *> _db_graph;
} // namespace graphquery::database
