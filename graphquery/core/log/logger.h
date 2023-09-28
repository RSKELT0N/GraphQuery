/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file log.hpp
* \brief log header file containing symbols to control
*       output towards to the program in different forms.
************************************************************/

#pragma once

#include "log.h"

#include <vector>
#include <string>
#include <memory>
#include <mutex>

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
    enum ELogType : uint8_t
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
        void Debug(std::string &) noexcept;

       /****************************************************************
       ** \brief Virtual info function for the logging system to call
       **        to render output towards the derived class.
       **
       ** \param const std::string & - Output to be rendered.
       ***************************************************************/
       void Info(std::string &) noexcept;

       /****************************************************************
       ** \brief Virtual warning function for the logging system to call
       **        to render output towards the derived class.
       **
       ** \param const std::string & - Output to be rendered.
       ***************************************************************/
       void Warning(std::string &) noexcept;

       /****************************************************************
       ** \brief Virtual error function for the logging system to call
       **        to render output towards the derived class.
       **
       ** \param const std::string & - Output to be rendered.
       ***************************************************************/
       void Error(std::string &) noexcept;

        /****************************************************************
        ** \brief Deleted copy constructor
        ***************************************************************/
        void Add_Logger(std::unique_ptr<ILog *>) noexcept;

    private:
        [[nodiscard]] std::string Format_Output(ELogType, std::string &) const noexcept;

    public:
        // ~ Log type to string conversion
        static constexpr const char* log_type_prefix[] = {"DEBUG",
                                                          "INFO",
                                                          "WARNING",
                                                          "ERROR"};

    private:
        // ~ Log level of the logging system
        ELogType m_level;
        // ~ mutex instance for logging to different derived classes.
        std::mutex m_mtx;
        // ~ array of loggers, which the log system holds and calls (Log).
        std::unique_ptr<std::vector<std::unique_ptr<ILog *>>> m_loggers;
    };
}