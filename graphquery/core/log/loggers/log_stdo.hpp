#pragma once

#include "log/logger.hpp"
#include "fmt/format.h"
#include "fmt/color.h"

namespace graphquery::logger
{
    class CLogSTDO : public ILog
    {
    public:
        CLogSTDO() = default;
        ~CLogSTDO() override;

    protected:

        /****************************************************************
        ** \brief Virtual debug function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void Debug(const std::string &) const noexcept override;

        /****************************************************************
        ** \brief Virtual debug function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void Info(const std::string &) const noexcept override;

        /****************************************************************
        ** \brief Virtual warning function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void Warning(const std::string &) const noexcept override;

        /****************************************************************
        ** \brief Virtual error function for the logging system to call
        **        to render output towards the derived class.
        **
        ** \param const std::string & - Output to be rendered.
        ***************************************************************/
        void Error(const std::string &) const noexcept override;

    private:
        [[nodiscard]] std::string Colourise(CLogSystem::ELogType type, const std::string &) const noexcept;
    };
}
