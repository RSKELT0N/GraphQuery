#pragma once

#include "log/logsystem/logsystem.h"
#include "fmt/format.h"
#include "fmt/color.h"

namespace graphquery::logger
{
    class CLogSTDO final : public ILog
    {
    public:
        CLogSTDO() = default;
        ~CLogSTDO() = default;

    protected:

        /****************************************************************
        ** \brief Virtual debug function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void Debug(std::string_view) noexcept override;

        /****************************************************************
        ** \brief Virtual debug function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void Info(std::string_view) noexcept override;

        /****************************************************************
        ** \brief Virtual warning function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void Warning(std::string_view) noexcept override;

        /****************************************************************
        ** \brief Virtual error function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void Error(std::string_view) noexcept override;

    private:
        [[nodiscard]] std::string Colourise(ELogType type, std::string_view) const noexcept;
    };
}

