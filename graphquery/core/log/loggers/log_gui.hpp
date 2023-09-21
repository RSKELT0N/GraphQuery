#pragma once

#include <log/logger.hpp>
#include <fmt/format.h>
#include <fmt/color.h>

namespace graphquery::logger
{
    class CLogGUI : public ILog
    {
    public:
        CLogGUI() = default;
        ~CLogGUI() override;

    protected:
        void Log(ELogType, const std::string &) const noexcept override;
    };
}
