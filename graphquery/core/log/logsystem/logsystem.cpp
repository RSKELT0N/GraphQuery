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
    this->m_backlog = std::make_shared<std::vector<SLogEntry>>();
}

std::shared_ptr<graphquery::logger::CLogSystem>
graphquery::logger::CLogSystem::get_instance() noexcept
{
    if (!m_log_system)
        m_log_system = std::shared_ptr<CLogSystem>(new CLogSystem());

    return m_log_system;
}

void
graphquery::logger::CLogSystem::add_logger(std::shared_ptr<ILog> logger) noexcept
{
    auto & _l = *this->m_loggers->emplace_back(std::move(logger));
    std::for_each(m_backlog->begin(), m_backlog->end(), [this, &_l](const auto & log) -> void { this->render_output(_l, log); });
}

void
graphquery::logger::CLogSystem::debug(const std::string_view out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if (this->m_level <= ELogType::debug)
    {
        std::string formatted = format_output(ELogType::debug, out.data());
        const auto & ref      = m_backlog->emplace_back(ELogType::debug, formatted);
        render_output(ref);
    }
}

void
graphquery::logger::CLogSystem::info(const std::string_view out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if (this->m_level <= ELogType::info)
    {
        std::string formatted = format_output(ELogType::info, out.data());
        const auto & ref      = m_backlog->emplace_back(ELogType::info, formatted);
        render_output(ref);
    }
}

void
graphquery::logger::CLogSystem::warning(const std::string_view out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if (this->m_level <= ELogType::warning)
    {
        std::string formatted = format_output(ELogType::warning, out.data());
        const auto & ref      = m_backlog->emplace_back(ELogType::warning, formatted);
        render_output(ref);
    }
}
void
graphquery::logger::CLogSystem::error(const std::string_view out) noexcept
{
    std::scoped_lock<std::mutex> lock(m_mtx);
    if (this->m_level <= ELogType::error)
    {
        std::string formatted = format_output(ELogType::error, out.data());
        const auto & ref      = m_backlog->emplace_back(ELogType::error, formatted);
        render_output(ref);
    }
}

void
graphquery::logger::CLogSystem::render_output(const SLogEntry & log) const noexcept
{
    switch (log._type)
    {
    case ELogType::debug: std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [formatted = log._str](auto & logger) -> void { logger->debug(formatted); }); break;
    case ELogType::info: std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [formatted = log._str](auto & logger) -> void { logger->info(formatted); }); break;
    case ELogType::warning:
        std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [formatted = log._str](auto & logger) -> void { logger->warning(formatted); });
        break;
    case ELogType::error: std::for_each(this->m_loggers->begin(), this->m_loggers->end(), [formatted = log._str](auto & logger) -> void { logger->error(formatted); }); break;
    }
}

void
graphquery::logger::CLogSystem::render_output(ILog & logger, const SLogEntry & log) noexcept
{
    switch (log._type)
    {
    case ELogType::debug: logger.debug(log._str); break;
    case ELogType::info: logger.info(log._str); break;
    case ELogType::warning: logger.warning(log._str); break;
    case ELogType::error: logger.error(log._str); break;
    }
}

std::string
graphquery::logger::CLogSystem::format_output(ELogType type, std::string out) const noexcept
{
    // ~ Get the current time of the log call.
    std::stringstream ss;
    const auto curr_time   = std::chrono::system_clock::now();
    const auto curr_time_c = std::chrono::system_clock::to_time_t(curr_time);

    ss << std::put_time(std::localtime(&curr_time_c), "%Y-%m-%d %X");
    std::string formatted_str = fmt::format("[{} {}] {}\n", log_type_prefix[static_cast<uint8_t>(type)], ss.str(), out);

    return formatted_str;
}
