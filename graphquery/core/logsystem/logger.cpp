#include "logger.hpp"

graphquery::logger::CLogSystem::CLogSystem()
{
    this->m_level = ELogType::LOG_SYSTEM_LEVEL;
    this->m_loggers = new std::vector<const ILog *>();
}

graphquery::logger::CLogSystem::~CLogSystem()
{
    delete this->m_loggers;
}

void
graphquery::logger::CLogSystem::Add_Logger(const ILog * logger) noexcept
{
    this->m_loggers->push_back(logger);
}

void
graphquery::logger::CLogSystem::Log(graphquery::logger::ELogType log_type, const std::string & out) const noexcept
{
    if(this->m_level <= log_type)
    {
        std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [log_type, out](const ILog *logger) {
            logger->Log(log_type, out);
        });
    }
}

void
graphquery::logger::CLogSystem::Assert(bool pred, graphquery::logger::ELogType log_type, const std::string & out) const noexcept
{
    if (!pred) this->Log(log_type, out);
}

