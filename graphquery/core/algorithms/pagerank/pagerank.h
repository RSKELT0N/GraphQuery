#pragma once

#include "db/analytic/algorithm.h"

namespace graphquery::database::analytic
{
    class CGraphAlgorithmPageRank final : public IGraphAlgorithm
    {
      public:
        CGraphAlgorithmPageRank();
        ~CGraphAlgorithmPageRank() override = default;

        [[nodiscard]] double compute(std::shared_ptr<storage::ILPGModel>) noexcept override;

      private:
        std::shared_ptr<logger::CLogSystem> m_log_system;
        static double sum(const std::shared_ptr<double[]> & vals, uint64_t size) noexcept;
        static double norm_diff(const std::shared_ptr<double[]> & _val, const std::shared_ptr<double[]> & __val, uint64_t size) noexcept;
    };
} // namespace graphquery::database::analytic
