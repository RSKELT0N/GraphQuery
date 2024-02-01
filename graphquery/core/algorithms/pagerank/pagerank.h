/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file pagerank.h
 * \brief PageRank is a analytic algorithm for measuring the
 *        the rank of each node, based on the popularity of
 *        incoming edges.
 ************************************************************/

#pragma once

#include "db/analytic/algorithm.h"

namespace graphquery::database::analytic
{
    class CGraphAlgorithmPageRank final : public IGraphAlgorithm
    {
      public:
        class CRelaxPR final : public graphquery::database::analytic::IRelax
        {
          public:
            CRelaxPR(const double _d, std::shared_ptr<uint32_t[]> _outdeg, std::shared_ptr<double[]> _x, std::shared_ptr<double[]> _y)
            {
                this->d      = _d;
                this->outdeg = std::move(_outdeg);
                this->x      = std::move(_x);
                this->y      = std::move(_y);
            }

            ~CRelaxPR() override = default;

            void relax(const uint64_t src, const uint64_t dst) noexcept override
            {
                const double w = d / outdeg[src];
                y[dst] += w * x[src];
            }

            double d;
            std::shared_ptr<uint32_t[]> outdeg;
            std::shared_ptr<double[]> x;
            std::shared_ptr<double[]> y;
        };

        explicit CGraphAlgorithmPageRank(std::string);
        ~CGraphAlgorithmPageRank() override = default;

        [[nodiscard]] double compute(storage::ILPGModel *) noexcept override;

      private:
        std::shared_ptr<logger::CLogSystem> m_log_system;
        static double sum(const std::shared_ptr<double[]> & vals, uint64_t size) noexcept;
        static double norm_diff(const std::shared_ptr<double[]> & _val, const std::shared_ptr<double[]> & __val, uint64_t size) noexcept;
    };
} // namespace graphquery::database::analytic
