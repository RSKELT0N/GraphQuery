/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file connected_components.h
 * \brief Connected components is a analytic algorithm to calculate
 *        the distinct number of isolated vertices. Also known as
 *        disjoint set, to determine the subset count of the vertices.
 ************************************************************/

#pragma once

#include "db/analytic/algorithm.h"

namespace graphquery::database::analytic
{
    class CGraphAlgorithmSSSP final : public IGraphAlgorithm
    {
      public:
        class CRelaxCC final : public graphquery::database::analytic::IRelax
        {
          public:
            CRelaxCC(std::shared_ptr<int[]> _x, std::shared_ptr<int[]> _y)
            {
                this->x = std::move(_x);
                this->y = std::move(_y);
            }

            ~CRelaxCC() override = default;

            void relax(const uint64_t src, const uint64_t dst) noexcept override { y[dst] = std::min(y[dst], y[src]); }

            std::shared_ptr<int[]> x;
            std::shared_ptr<int[]> y;
        };

        explicit CGraphAlgorithmSSSP(std::string, const std::shared_ptr<logger::CLogSystem> &);
        ~CGraphAlgorithmSSSP() override = default;

        [[nodiscard]] double compute(storage::ILPGModel *) noexcept override;
    };
} // namespace graphquery::database::analytic
