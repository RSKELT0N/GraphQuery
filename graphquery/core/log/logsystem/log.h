#pragma once

#include <string_view>

namespace graphquery::logger
{
    /****************************************************************
    ** \class ILog
    ** \brief Log class that is added towards the logging system,
    **        providing an API for the rest of the system to call.
    ***************************************************************/
    class ILog
    {
      protected:
        /****************************************************************
        ** \brief Creates an instance of the class (for derived classes).
        ***************************************************************/
        ILog() = default;

        /****************************************************************
        ** \brief (Rule of five)
        ***************************************************************/
        virtual ~ILog()                = default;
        ILog(ILog &&)                  = default;
        ILog(const ILog &)             = default;
        ILog & operator=(const ILog &) = default;
        ILog & operator=(ILog &&)      = default;

        /****************************************************************
        ** \brief Virtual debug function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        virtual void debug(std::string_view) noexcept = 0;

        /****************************************************************
         * \brief Virtual debug function for the logging system to call
         *        to render output towards the derived class.
         *
         * \param const std::string & - Output to be rendered.
         **************************************************************/
        virtual void info(std::string_view) noexcept = 0;

        /****************************************************************
        * \brief Virtual warning function for the logging system to call
        *        to render output towards the derived class.
        *
        aram const std::string & - Output to be rendered.
        ***** \p**********************************************************/
        virtual void warning(std::string_view) noexcept = 0;

        /****************************************************************
         * \brief Virtual error function for the logging system to call
         *        to render output towards the derived class.
         *
         * \param const std::string & - Output to be rendered.
         **************************************************************/
        virtual void error(std::string_view) noexcept = 0;

        // ~ friend reference of the logging system (can call protected/private
        // functionality)
        friend class CLogSystem;
    };
} // namespace graphquery::logger
