#include "logger.h"

#include "fmt/format.h"

#include <algorithm>
#include <mutex>

graphquery::logger::CLogSystem::CLogSystem()
{
    this->m_level = ELogType::LOG_SYSTEM_LEVEL;
    this->m_loggers = std::make_unique<std::vector<std::shared_ptr<ILog>>>();
}

void
graphquery::logger::CLogSystem::Add_Logger(std::shared_ptr<ILog> logger) noexcept
{
    this->m_loggers->emplace_back(logger);
}

void graphquery::logger::CLogSystem::Debug(const std::string & out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if(this->m_level <= ELogType::debug)
    {
       std::string formatted = Format_Output(ELogType::debug, out);
       std::ranges::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&formatted] (auto & logger) -> void
       {
           logger->Debug(formatted);
       });
    }
}

void graphquery::logger::CLogSystem::Info(const std::string & out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if(this->m_level <= ELogType::info)
    {
        std::string formatted = Format_Output(ELogType::info, out);
        std::ranges::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&formatted] (auto & logger) -> void
        {
            logger->Info(formatted);
        });
    }
}

void graphquery::logger::CLogSystem::Warning(const std::string & out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if(this->m_level <= ELogType::warning)
    {
        std::string formatted = Format_Output(ELogType::warning, out);
        std::ranges::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&formatted] (auto & logger) -> void
        {
            logger->Warning(formatted);
        });
    }
}
void graphquery::logger::CLogSystem::Error(const std::string & out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if(this->m_level <= ELogType::error)
    {
        std::string formatted = Format_Output(ELogType::error, out);
        std::ranges::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&formatted] (auto & logger) -> void
        {
            logger->Error(formatted);
        });
    }
}

std::string graphquery::logger::CLogSystem::Format_Output(ELogType type, const std::string & out) const noexcept
{
    // ~ Get the current time of the log call.
    auto curr_time = std::chrono::system_clock::now();
    auto curr_time_c = std::chrono::system_clock::to_time_t(curr_time);

    std::string formatted_str = fmt::format("[%s (%s)]", curr_time_c, CLogSystem::log_type_prefix[static_cast<uint8_t>(type)], out);
    return formatted_str;
}

