#include "log_stdo.h"

#include "fmt/format.h"

void
graphquery::logger::CLogSTDO::debug(std::string_view out) noexcept
{
    std::string formatted = colourise(ELogType::debug, out);
    fmt::print("{}", formatted);
}

void
graphquery::logger::CLogSTDO::info(std::string_view out) noexcept
{
    std::string formatted = colourise(ELogType::info, out);
    fmt::print("{}", formatted);
}

void
graphquery::logger::CLogSTDO::warning(std::string_view out) noexcept
{
    std::string formatted = colourise(ELogType::warning, out);
    fmt::print("{}", formatted);
}

void
graphquery::logger::CLogSTDO::error(std::string_view out) noexcept
{
    std::string formatted = colourise(ELogType::error, out);
    fmt::print("{}", formatted);
}

std::string
graphquery::logger::CLogSTDO::colourise(graphquery::logger::ELogType type, std::string_view out) const noexcept
{
    std::string formatted = {};
    switch (type)
    {
    case ELogType::debug: formatted = fmt::format(fg(fmt::color::cyan), out); break;
    case ELogType::info: formatted = fmt::format(fg(fmt::color::light_green), out); break;
    case ELogType::warning: formatted = fmt::format(fg(fmt::color::yellow), out); break;
    case ELogType::error: formatted = fmt::format(fg(fmt::color::red), out); break;
    }

    return formatted;
}
