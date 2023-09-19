#pragma once

#include <logger.hpp>
#include <fmt/format.h>
#include <fmt/color.h>

namespace graphquery::logger
{
    class CLogSTDO : public ILog
    {
    public:
        CLogSTDO() = default;
        ~CLogSTDO() override;

    protected:
        void Log(ELogType, const std::string &) const noexcept override;
    };
}
