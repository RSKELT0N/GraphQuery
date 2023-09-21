#include "logger.hpp"

#include <algorithm>
#include <iostream>

graphquery::logger::CLogSystem::CLogSystem()
{
    this->m_level = ELogType::LOG_SYSTEM_LEVEL;
    this->m_loggers = new std::vector<std::unique_ptr<ILog *>>();
}

graphquery::logger::CLogSystem::~CLogSystem()
{
    delete this->m_loggers;
}

void
graphquery::logger::CLogSystem::Add_Logger(ILog * logger) noexcept
{
    this->m_loggers->emplace_back(std::make_unique<ILog *>(logger));
}

void
graphquery::logger::CLogSystem::Log(graphquery::logger::ELogType log_type, const std::string & out) const noexcept
{
    if(this->m_level <= log_type)
    {
        std::for_each(this->m_loggers->begin(),
                      this->m_loggers->end(),
                      [log_type, out](std::unique_ptr<ILog *> & logger) {
                            (*logger)->Log(log_type, out);
                      });
    }
}

void
graphquery::logger::CLogSystem::Assert(bool pred, graphquery::logger::ELogType log_type, const std::string & out) const noexcept
{
    if (!pred) this->Log(log_type, out);
}

