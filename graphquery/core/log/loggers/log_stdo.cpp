#include "log_stdo.h"

#include "fmt/format.h"

void graphquery::logger::CLogSTDO::Debug(const std::string & out) const noexcept
{
    std::string formatted = Colourise(ELogType::debug, out);
    fmt::print("{}", formatted);
}

void graphquery::logger::CLogSTDO::Info(const std::string & out) const noexcept
{
    std::string formatted = Colourise(ELogType::info, out);
    fmt::print("{}", formatted);
}

void graphquery::logger::CLogSTDO::Warning(const std::string & out) const noexcept
{
    std::string formatted = Colourise(ELogType::warning, out);
    fmt::print("{}", formatted);
}

void graphquery::logger::CLogSTDO::Error(const std::string & out) const noexcept
{
    std::string formatted = Colourise(ELogType::error, out);
    fmt::print("{}", formatted);
}

std::string graphquery::logger::CLogSTDO::Colourise(graphquery::logger::ELogType type, const std::string &) const noexcept
{
    std::string formatted = {};
    switch(type)
    {
        case ELogType::debug:
            formatted = fmt::format("{}", fmt::format(fg(fmt::color::cyan), "DEBUG"));
            break;
        case ELogType::info:
            formatted = fmt::format("{}", fmt::format(fg(fmt::color::green), "INFO"));
            break;
        case ELogType::warning:
            formatted = fmt::format("{}", fmt::format(fg(fmt::color::yellow), "WARNING"));
            break;
        case ELogType::error:
            formatted = fmt::format("{}", fmt::format(fg(fmt::color::red), "ERROR"));
            break;
    }

    return formatted;
}
