#pragma once

#include "fmt/color.h"
#include "log/logsystem/logsystem.h"

namespace graphquery::logger
{
    class CLogSTDO final : public ILog
    {
      public:
        CLogSTDO()  = default;
        ~CLogSTDO() override = default;

      protected:
        /****************************************************************
        ** \brief Virtual debug function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void debug(std::string_view) noexcept override;

        /****************************************************************
        ** \brief Virtual debug function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void info(std::string_view) noexcept override;

        /****************************************************************
        ** \brief Virtual warning function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void warning(std::string_view) noexcept override;

        /****************************************************************
        ** \brief Virtual error function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void error(std::string_view) noexcept override;

      private:
        [[nodiscard]] std::string colourise(ELogType type, std::string_view) const noexcept;
    };
} // namespace graphquery::logger
