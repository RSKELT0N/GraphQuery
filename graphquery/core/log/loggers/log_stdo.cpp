#include "log_stdo.hpp"

graphquery::logger::CLogSTDO::~CLogSTDO() {
    ILog::~ILog();
}

void graphquery::logger::CLogSTDO::Debug(const std::string & out) const noexcept
{
    std::string formatted = Colourise(CLogSystem::ELogType::debug, out);
    fmt::print("{}", formatted);
}

void graphquery::logger::CLogSTDO::Info(const std::string & out) const noexcept
{
    std::string formatted = Colourise(CLogSystem::ELogType::info, out);
    fmt::print("{}", formatted);
}

void graphquery::logger::CLogSTDO::Warning(const std::string & out) const noexcept
{
    std::string formatted = Colourise(CLogSystem::ELogType::warning, out);
    fmt::print("{}", formatted);
}

void graphquery::logger::CLogSTDO::Error(const std::string & out) const noexcept
{
    std::string formatted = Colourise(CLogSystem::ELogType::error, out);
    fmt::print("{}", formatted);
}

std::string graphquery::logger::CLogSTDO::Colourise(graphquery::logger::CLogSystem::ELogType type, const std::string &) const noexcept
{
    std::string formatted = {};
    switch(type)
    {
        case CLogSystem::ELogType::debug:
            formatted = fmt::format("{}", fmt::format(fg(fmt::color::cyan), "DEBUG"));
            break;
        case CLogSystem::ELogType::info:
            formatted = fmt::format("{}", fmt::format(fg(fmt::color::green), "INFO"));
            break;
        case CLogSystem::ELogType::warning:
            formatted = fmt::format("{}", fmt::format(fg(fmt::color::yellow), "WARNING"));
            break;
        case CLogSystem::ELogType::error:
            formatted = fmt::format("{}", fmt::format(fg(fmt::color::red), "ERROR"));
            break;
    }

    return formatted;
}
