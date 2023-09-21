/************************************************************
* \author Ryan Skelton                                      *
* \date 18/09/2023                                          *
* \file log.hpp                                          *
* \brief log header file containing symbols to control   *
*       output towards to the program in different forms.   *
************************************************************/

#pragma once

#include <vector>
#include <string>

#ifndef NDEBUG
#define LOG_SYSTEM_LEVEL debug
#else
#define LOG_SYSTEM_LEVEL info
#endif

namespace graphquery::logger
{

    enum ELogType : int8_t
    {
        debug = -1,
        info = 0,
        warning = 1,
        error = 2
    };

    class ILog
    {
    public:
        ILog() = default;
        virtual ~ILog() = default;

        ILog(ILog &&) = delete;
        ILog(const ILog &) = delete;
        ILog & operator=(const ILog &) = delete;
        ILog & operator=(ILog &&) = delete;

    protected:
        friend class CLogSystem;
        virtual void Log(ELogType, const std::string &) const noexcept = 0;
    };

    class CLogSystem
    {
    public:
        explicit CLogSystem();
        ~CLogSystem();

        CLogSystem(CLogSystem &&) = delete;
        CLogSystem(const CLogSystem &) = delete;
        CLogSystem & operator=(const CLogSystem &) = delete;
        CLogSystem & operator=(CLogSystem &&) = delete;

    public:
        void Log(ELogType, const std::string &) const noexcept;
        void Assert(bool, ELogType, const std::string &) const noexcept;
        void Add_Logger(const ILog *) noexcept;

    private:
        ELogType m_level;
        std::vector<const ILog *> *m_loggers;
    };
}