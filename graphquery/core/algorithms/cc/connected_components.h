#pragma once

#include "db/analytic/algorithm.h"

namespace graphquery::database::analytic
{
    class CGraphAlgorithmCC final : public IGraphAlgorithm
    {
      public:
        CGraphAlgorithmCC();
        ~CGraphAlgorithmCC() override = default;

        [[nodiscard]] double compute(std::shared_ptr<storage::ILPGModel>) noexcept override;

      private:
        std::shared_ptr<logger::CLogSystem> m_log_system;
    };
} // namespace graphquery::database::analytic
