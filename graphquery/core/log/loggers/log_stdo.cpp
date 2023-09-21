#include "log_stdo.hpp"

graphquery::logger::CLogSTDO::~CLogSTDO() {
    ILog::~ILog();
}

void
graphquery::logger::CLogSTDO::Log(graphquery::logger::ELogType log_type, const std::string& out) const noexcept
{
    std::string ret;

    switch(log_type)
    {
        case ELogType::debug:
            ret += fmt::format("[{}]", fmt::format(fg(fmt::color::cyan), "DEBUG"));
            break;
        case ELogType::info:
            ret += fmt::format("[{}]", fmt::format(fg(fmt::color::green), "INFO"));
            break;
        case ELogType::warning:
            ret += fmt::format("[{}]", fmt::format(fg(fmt::color::yellow), "WARNING"));
            break;
        case ELogType::error:
            ret += fmt::format("[{}]", fmt::format(fg(fmt::color::red), "ERROR"));
            break;
    }

    fmt::print("{} {}\n", ret, out);
}