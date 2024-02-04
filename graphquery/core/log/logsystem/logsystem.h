/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file logsystem.h
 * \brief log header file containing symbols to control
 *       output towards to the program in different forms.
 ************************************************************/

#pragma once

#include "log.h"

#include "fmt/format.h"

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#ifndef NDEBUG
#define LOG_SYSTEM_LEVEL debug
#else
#define LOG_SYSTEM_LEVEL info
#endif

namespace graphquery::logger
{
    /****************************************************************
    ** \brief Log types available towards the logging system.
    **        Marked with uint8_t to represent output level, which
    **        output will be determined at run-time.
    ***************************************************************/
    enum class ELogType : uint8_t
    {
        // ~ more information (debugging this program)
        debug = 0,
        // ~ standard logging level
        info = 1,
        // ~ logging for occurring issue that can be resolved.
        warning = 2,
        // ~ error has occurred within the system.
        error = 3
    };

    /****************************************************************
    ** \class CLogSystem
    ** \brief Final logging system which handles a vector of derived
    **        ILog pointers to render with different functionality.
    ***************************************************************/
    class CLogSystem final
    {
      private:
        /****************************************************************
        ** \brief Creates an instance of the class (for derived classes).
        ***************************************************************/
        explicit CLogSystem();

      public:
        static std::shared_ptr<CLogSystem> get_instance() noexcept;
        ~CLogSystem() = default;

        /****************************************************************
        ** \brief Deleted special functions
        ***************************************************************/
        CLogSystem(CLogSystem &&)                  = delete;
        CLogSystem & operator=(CLogSystem &&)      = delete;
        CLogSystem & operator=(const CLogSystem &) = delete;
        CLogSystem(const CLogSystem &)             = delete;

        /****************************************************************
        ** \brief Virtual debug function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void debug(std::string_view) noexcept;

        /****************************************************************
        ** \brief Virtual info function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void info(std::string_view) noexcept;

        /****************************************************************
        ** \brief Virtual warning function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void warning(std::string_view) noexcept;

        /****************************************************************
        ** \brief Virtual error function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void error(std::string_view) noexcept;

        /****************************************************************
        ** \brief To add an instance of ILog towards the system logger
        **
        ** \param std::unique_ptr<ILog> - Instance of the ILog class
        ***************************************************************/
        void add_logger(std::shared_ptr<ILog>) noexcept;

      private:
        struct SLogEntry
        {
            const ELogType _type;
            const std::string _str;

            SLogEntry(const ELogType _t, const std::string_view _s): _type(_t), _str(_s) {}
        };

        static std::shared_ptr<CLogSystem> m_log_system;
        void render_output(const SLogEntry &) const noexcept;
        static void render_output(ILog &, const SLogEntry &) noexcept;
        [[nodiscard]] std::string format_output(ELogType, std::string) const noexcept;

        std::vector<std::string> log_type_prefix = {"DEBUG", // ~ Log type to string conversion
                                                    "INFO",
                                                    "WARNING",
                                                    "ERROR"};
        ELogType m_level;                                              // ~ Log level of the logging system
        std::mutex m_mtx;                                              // ~ mutex instance for logging to different derived classes.
        std::unique_ptr<std::vector<std::shared_ptr<ILog>>> m_loggers; // ~ array of loggers, which the log system holds and calls (Log).
        std::shared_ptr<std::vector<SLogEntry>> m_backlog;             // ~ backlog of all messages for loggers to be updated with when added.
    };
} // namespace graphquery::logger