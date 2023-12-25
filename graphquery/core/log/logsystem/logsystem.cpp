#include "logsystem.h"

#include <algorithm>
#include <iomanip>
#include <mutex>
#include <sstream>

//~ static symbol link
std::shared_ptr<graphquery::logger::CLogSystem> graphquery::logger::CLogSystem::m_log_system;

graphquery::logger::CLogSystem::CLogSystem()
{
    this->m_level   = ELogType::LOG_SYSTEM_LEVEL;
    this->m_loggers = std::make_unique<std::vector<std::shared_ptr<ILog>>>();
}

std::shared_ptr<graphquery::logger::CLogSystem>
graphquery::logger::CLogSystem::get_instance() noexcept
{
    if (!m_log_system)
    {
        m_log_system = std::shared_ptr<CLogSystem>(new CLogSystem());
    }
    return m_log_system;
}

void
graphquery::logger::CLogSystem::add_logger(std::shared_ptr<ILog> logger) noexcept
{
    this->m_loggers->emplace_back(logger);
}

void
graphquery::logger::CLogSystem::debug(std::string_view out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if (this->m_level <= ELogType::debug)
    {
        std::string formatted = format_output(ELogType::debug, out);
        std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&formatted](auto & logger) -> void { logger->debug(formatted); });
    }
}

void
graphquery::logger::CLogSystem::info(std::string_view out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if (this->m_level <= ELogType::info)
    {
        std::string formatted = format_output(ELogType::info, out);
        std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&formatted](auto & logger) -> void { logger->info(formatted); });
    }
}

void
graphquery::logger::CLogSystem::warning(std::string_view out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if (this->m_level <= ELogType::warning)
    {
        std::string formatted = format_output(ELogType::warning, out);
        std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&formatted](auto & logger) -> void { logger->warning(formatted); });
    }
}
void
graphquery::logger::CLogSystem::error(std::string_view out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if (this->m_level <= ELogType::error)
    {
        std::string formatted = format_output(ELogType::error, out);
        std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [&formatted](auto & logger) -> void { logger->error(formatted); });
    }
}

std::string
graphquery::logger::CLogSystem::format_output(ELogType type, std::string_view out) const noexcept
{
    // ~ Get the current time of the log call.
    std::stringstream ss;
    auto curr_time   = std::chrono::system_clock::now();
    auto curr_time_c = std::chrono::system_clock::to_time_t(curr_time);

    ss << std::put_time(std::localtime(&curr_time_c), "%Y-%m-%d %X");
    std::string formatted_str = fmt::format("[{} {}] {}\n", CLogSystem::log_type_prefix[static_cast<uint8_t>(type)], ss.str(), out);
    return formatted_str;
}
