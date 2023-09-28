#include "logger.h"

#include "fmt/format.h"

#include <algorithm>
#include <ctime>
#include <mutex>

graphquery::logger::CLogSystem::CLogSystem()
{
    this->m_level = ELogType::LOG_SYSTEM_LEVEL;
    this->m_loggers = std::make_unique<std::vector<std::unique_ptr<ILog *>>>();
}

void
graphquery::logger::CLogSystem::Add_Logger(std::unique_ptr<ILog *> logger) noexcept
{
    this->m_loggers->emplace_back(std::move(logger));
}

void graphquery::logger::CLogSystem::Debug(std::string & out) noexcept
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if(this->m_level <= ELogType::debug)
    {
       std::string formatted = Format_Output(ELogType::debug, out);
       std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&out] (auto & logger) -> void
       {
           (*logger)->Debug(out);
       });
    }
}

void graphquery::logger::CLogSystem::Info(std::string & out) noexcept
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if(this->m_level <= ELogType::info)
    {
        std::string formatted = Format_Output(ELogType::info, out);
        std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&out] (auto & logger) -> void
        {
            (*logger)->Info(out);
        });
    }
}

void graphquery::logger::CLogSystem::Warning(std::string & out) noexcept
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if(this->m_level <= ELogType::warning)
    {
        std::string formatted = Format_Output(ELogType::warning, out);
        std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&out] (auto & logger) -> void
        {
            (*logger)->Warning(out);
        });
    }
}
void graphquery::logger::CLogSystem::Error(std::string & out) noexcept
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if(this->m_level <= ELogType::error)
    {
        std::string formatted = Format_Output(ELogType::error, out);
        std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&out] (auto & logger) -> void
        {
            (*logger)->Error(out);
        });
    }
}

std::string graphquery::logger::CLogSystem::Format_Output(ELogType type, std::string & out) const noexcept
{
    // ~ Get the current time of the log call.
    time_t curr_time = time(nullptr);
    const char * curr_time_c = ctime(&curr_time);

    std::string formatted_str = fmt::format("[%s (%s)]", curr_time_c, CLogSystem::log_type_prefix[type], out);
    return formatted_str;
}

