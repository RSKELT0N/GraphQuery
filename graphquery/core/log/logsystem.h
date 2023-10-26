/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file logsystem.h
* \brief log header file containing symbols to control
*       output towards to the program in different forms.
************************************************************/

#pragma once

#include "log.h"

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <string_view>

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

    public:
        /****************************************************************
        ** \brief Creates an instance of the class (for derived classes).
        ***************************************************************/
        explicit CLogSystem();
        ~CLogSystem() = default;

    public:
        /****************************************************************
        ** \brief Deleted special functions
        ***************************************************************/
        CLogSystem(CLogSystem &&) = delete;
        CLogSystem & operator=(CLogSystem &&) = delete;
        CLogSystem & operator=(const CLogSystem &) = delete;
        CLogSystem(const CLogSystem &) = delete;

        /****************************************************************
        ** \brief Virtual debug function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void Debug(std::string_view) noexcept;

       /****************************************************************
       ** \brief Virtual info function for the logging system to call
       **        to render output towards the derived class.
       **
       ** \param const std::string & - Output to be rendered.
       ***************************************************************/
       void Info(std::string_view) noexcept;

       /****************************************************************
       ** \brief Virtual warning function for the logging system to call
       **        to render output towards the derived class.
       **
       ** \param const std::string & - Output to be rendered.
       ***************************************************************/
       void Warning(std::string_view) noexcept;

       /****************************************************************
       ** \brief Virtual error function for the logging system to call
       **        to render output towards the derived class.
       **
       ** \param const std::string & - Output to be rendered.
       ***************************************************************/
       void Error(std::string_view) noexcept;

        /****************************************************************
        ** \brief To add an instance of ILog towards the system logger
        **
        ** \param std::unique_ptr<ILog> - Instance of the ILog class
        ***************************************************************/
        void Add_Logger(std::shared_ptr<ILog>) noexcept;

    private:
        [[nodiscard]] std::string Format_Output(ELogType, std::string_view) const noexcept;

    private:
        // ~ Log type to string conversion
        std::vector<std::string> log_type_prefix = {"DEBUG",
                                                    "INFO",
                                                    "WARNING",
                                                    "ERROR"};
        // ~ Log level of the logging system
        ELogType m_level;
        // ~ mutex instance for logging to different derived classes.
        std::mutex m_mtx;
        // ~ array of loggers, which the log system holds and calls (Log).
        std::unique_ptr<std::vector<std::shared_ptr<ILog>>> m_loggers;
    };
}