#include "log_stdo.h"

#include "fmt/format.h"

void graphquery::logger::CLogSTDO::Debug(std::string_view out) noexcept
{
    std::string formatted = Colourise(ELogType::debug, out);
    fmt::print("{}", formatted);
}

void graphquery::logger::CLogSTDO::Info(std::string_view out) noexcept
{
    std::string formatted = Colourise(ELogType::info, out);
    fmt::print("{}", formatted);
}

void graphquery::logger::CLogSTDO::Warning(std::string_view out) noexcept
{
    std::string formatted = Colourise(ELogType::warning, out);
    fmt::print("{}", formatted);
}

void graphquery::logger::CLogSTDO::Error(std::string_view out) noexcept
{
    std::string formatted = Colourise(ELogType::error, out);
    fmt::print("{}", formatted);
}

std::string graphquery::logger::CLogSTDO::Colourise(graphquery::logger::ELogType type, std::string_view out) const noexcept
{
    std::string formatted = {};
    switch(type)
    {
        case ELogType::debug:
            formatted = fmt::format(fg(fmt::color::cyan), out);
            break;
        case ELogType::info:
            formatted = fmt::format(fg(fmt::color::light_green), out);
            break;
        case ELogType::warning:
            formatted = fmt::format(fg(fmt::color::yellow), out);
            break;
        case ELogType::error:
            formatted = fmt::format(fg(fmt::color::red), out);
            break;
    }

    return formatted;
}
