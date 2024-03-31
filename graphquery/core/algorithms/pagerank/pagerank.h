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
        class CRelaxPR final : public IRelax
        {
        public:
            CRelaxPR(const double _d, uint32_t _outdeg[], double _x[], double _y[])
            {
                this->d      = _d;
                this->outdeg = _outdeg;
                this->x      = _x;
                this->y      = _y;
            }

            ~CRelaxPR() override = default;

            void relax(const storage::Id_t src, const storage::Id_t dst) noexcept override
            {
                const double w = d / outdeg[src] * x[src];
                auto & y_atomic = reinterpret_cast<std::atomic<double> &>(y[dst]);
                y_atomic += w;
            }

            double d;
            uint32_t * outdeg;
            double * x;
            double * y;
        };

        explicit CGraphAlgorithmPageRank(std::string, const std::shared_ptr<logger::CLogSystem> &);
        ~CGraphAlgorithmPageRank() override = default;

        [[nodiscard]] double compute(storage::IModel *) const noexcept override;

    private:
        static double sum(const double vals[], const storage::Id_t sparse[], uint64_t size) noexcept;
        static double norm_diff(const double _val[], const double __val[], storage::Id_t sparse[], uint64_t size) noexcept;
    };
} // namespace graphquery::database::analytic