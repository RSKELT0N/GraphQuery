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
            CRelaxCC(int _x[], int _y[])
            {
                this->x = _x;
                this->y = _y;
            }

            ~CRelaxCC() override = default;

            void relax(const int64_t src, const int64_t dst) noexcept override
            {
                int y_dst, y_src, y_dst_n;
                do
                {
                    y_dst = y[dst];
                    y_src = y[src];
                    y_dst_n = std::min(y_dst, y_src);
                } while(!utils::atomic_fetch_cas(&y[dst], y_dst, y_dst_n));
            }

            int * x;
            int * y;
        };

        explicit CGraphAlgorithmSSSP(std::string, const std::shared_ptr<logger::CLogSystem> &);
        ~CGraphAlgorithmSSSP() override = default;

        [[nodiscard]] double compute(storage::ILPGModel *) const noexcept override;
    };
} // namespace graphquery::database::analytic
