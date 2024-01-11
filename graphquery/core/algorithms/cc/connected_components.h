#pragma once

#include "db/analytic/algorithm.h"

namespace graphquery::database::analytic
{
    class CGraphAlgorithmCC final : public IGraphAlgorithm
    {
      public:
        explicit CGraphAlgorithmCC(std::string);
        ~CGraphAlgorithmCC() override = default;

        [[nodiscard]] double compute(storage::ILPGModel *) noexcept override;

      private:
        std::shared_ptr<logger::CLogSystem> m_log_system;
    };
} // namespace graphquery::database::analytic
