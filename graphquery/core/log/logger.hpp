/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file log.hpp
* \brief log header file containing symbols to control
*       output towards to the program in different forms.
************************************************************/

#pragma once

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
    ** \class ILog
    ** \brief Log class that is added towards the logging system,
    **        providing an API for the rest of the system to call.
    ***************************************************************/
    class ILog
    {
    public:

        /****************************************************************
        ** \brief Deleted move constructor.
        ***************************************************************/
        ILog(ILog &&) = delete;

        /****************************************************************
        ** \brief Deleted copy constructor
        ***************************************************************/
        ILog(const ILog &) = delete;

        /****************************************************************
        ** \brief Deleted copy assignment.
        ***************************************************************/
        ILog & operator=(const ILog &) = delete;

        /****************************************************************
        ** \brief Deleted move assignment.
        ***************************************************************/
        ILog & operator=(ILog &&) = delete;

    protected:

        /****************************************************************
        ** \brief Creates an instance of the class (for derived classes).
        ***************************************************************/
        ILog() = default;

        /****************************************************************
        ** \brief Virtual deconstructor to clean up class. For derived
        **        classes to override and delete member variables.
        ***************************************************************/
        virtual ~ILog() = default;

    protected:

        /****************************************************************
        ** \brief Virtual debug function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        virtual void Debug(const std::string &) const noexcept = 0;

        /****************************************************************
        * \brief Virtual debug function for the logging system to call
        *        to render output towards the derived class.
        *
        * \param const std::string & - Output to be rendered.
        **************************************************************/
        virtual void Info(const std::string &) const noexcept = 0;

        /****************************************************************
        * \brief Virtual warning function for the logging system to call
        *        to render output towards the derived class.
        *
        aram const std::string & - Output to be rendered.
        ***** \p**********************************************************/
        virtual void Warning(const std::string &) const noexcept = 0;
        /****************************************************************
        * \brief Virtual error function for the logging system to call
        *        to render output towards the derived class.
        *
        * \param const std::string & - Output to be rendered.
        **************************************************************/
        virtual void Error(const std::string &) const noexcept = 0;

    protected:
        // ~ friend reference of the logging system (can call protected/private functionality)
        friend class CLogSystem;
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
        ** \brief Log types available towards the logging system.
         *        Marked with uint8_t to represent output level, which
         *        output will be determined at run-time.
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
        ** \brief Creates an instance of the class (for derived classes).
        ***************************************************************/
        explicit CLogSystem();

        /****************************************************************
        ** \brief Destroys an instance of the class.
        ***************************************************************/
        ~CLogSystem();

        /****************************************************************
        ** \brief Deleted move constructor
        ***************************************************************/
        CLogSystem(CLogSystem &&) = delete;

        /****************************************************************
        ** \brief Deleted copy constructor
        ***************************************************************/
        CLogSystem(const CLogSystem &) = delete;

        /****************************************************************
        ** \brief Deleted copy assignment
        ***************************************************************/
        CLogSystem & operator=(const CLogSystem &) = delete;

        /****************************************************************
        ** \brief Deleted move assignment
        ***************************************************************/
        CLogSystem & operator=(CLogSystem &&) = delete;

    public:

        /****************************************************************
        ** \brief Virtual debug function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void Debug(std::string &) const noexcept;

       /****************************************************************
       ** \brief Virtual info function for the logging system to call
       **        to render output towards the derived class.
       **
       ** \param const std::string & - Output to be rendered.
       ***************************************************************/
       void Info(std::string &) const noexcept;

       /****************************************************************
       ** \brief Virtual warning function for the logging system to call
       **        to render output towards the derived class.
       **
       ** \param const std::string & - Output to be rendered.
       ***************************************************************/
       void Warning(std::string &) const noexcept;

       /****************************************************************
       ** \brief Virtual error function for the logging system to call
       **        to render output towards the derived class.
       **
       ** \param const std::string & - Output to be rendered.
       ***************************************************************/
       void Error(std::string &) const noexcept;

        /****************************************************************
        ** \brief Deleted copy constructor
        ***************************************************************/
        void Add_Logger(ILog *) noexcept;

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
        // ~ array of loggers, which the log system holds and calls (Log).
        std::vector<std::unique_ptr<ILog *>> * m_loggers;
    };
}