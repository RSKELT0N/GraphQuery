#include "logsystem.h"

#include <algorithm>
#include <mutex>
#include <iomanip>
#include <sstream>

//~ static symbol link
std::shared_ptr<graphquery::logger::CLogSystem> graphquery::logger::CLogSystem::m_log_system;

graphquery::logger::CLogSystem::CLogSystem()
{
    this->m_level = ELogType::LOG_SYSTEM_LEVEL;
    this->m_loggers = std::make_unique<std::vector<std::shared_ptr<ILog>>>();
}

std::shared_ptr<graphquery::logger::CLogSystem>
graphquery::logger::CLogSystem::GetInstance() noexcept
{
    if(!m_log_system)
    {
        m_log_system = std::shared_ptr<CLogSystem>(new CLogSystem());
    }
    return m_log_system;
}

void
graphquery::logger::CLogSystem::Add_Logger(std::shared_ptr<ILog> logger) noexcept
{
    this->m_loggers->emplace_back(logger);
}

void graphquery::logger::CLogSystem::Debug(std::string_view out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if(this->m_level <= ELogType::debug)
    {
       std::string formatted = Format_Output(ELogType::debug, out);
       std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&formatted] (auto & logger) -> void
       {
           logger->Debug(formatted);
       });
    }
}

void graphquery::logger::CLogSystem::Info(std::string_view out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if(this->m_level <= ELogType::info)
    {
        std::string formatted = Format_Output(ELogType::info, out);
        std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&formatted] (auto & logger) -> void
        {
            logger->Info(formatted);
        });
    }
}

void graphquery::logger::CLogSystem::Warning(std::string_view out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if(this->m_level <= ELogType::warning)
    {
        std::string formatted = Format_Output(ELogType::warning, out);
        std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&formatted] (auto & logger) -> void
        {
            logger->Warning(formatted);
        });
    }
}
void graphquery::logger::CLogSystem::Error(std::string_view out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if(this->m_level <= ELogType::error)
    {
        std::string formatted = Format_Output(ELogType::error, out);
        std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&formatted] (auto & logger) -> void
        {
            logger->Error(formatted);
        });
    }
}

std::string graphquery::logger::CLogSystem::Format_Output(ELogType type, std::string_view out) const noexcept
{
    // ~ Get the current time of the log call.
    std::stringstream ss;
    auto curr_time = std::chrono::system_clock::now();
    auto curr_time_c = std::chrono::system_clock::to_time_t(curr_time);

    ss << std::put_time(std::localtime(&curr_time_c), "%Y-%m-%d %X");
    std::string formatted_str = fmt::format("[{} {}] {}\n", CLogSystem::log_type_prefix[static_cast<uint8_t>(type)], ss.str(), out);
    return formatted_str;
}
