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
    class CGraphAlgorithmWCC final : public IGraphAlgorithm
    {
      public:
        class CRelaxCC final : public IRelax
        {
          public:
            CRelaxCC(storage::Id_t _x[], storage::Id_t _y[])
            {
                this->x = _x;
                this->y = _y;
            }

            ~CRelaxCC() override = default;

            void relax(const storage::Id_t src, const storage::Id_t dst) noexcept override
            {
                if(y[dst] <= y[src]) return;

                storage::Id_t y_dst, y_src, y_dst_n;
                do
                {
                    y_dst = y[dst];
                    y_src = y[src];
                    y_dst_n = std::min(y_dst, y_src);
                } while (!utils::atomic_fetch_cas(&y[dst], y_dst, y_dst_n, true));
            }

            storage::Id_t * x;
            storage::Id_t * y;
        };

        explicit CGraphAlgorithmWCC(std::string, const std::shared_ptr<logger::CLogSystem> &);
        ~CGraphAlgorithmWCC() override = default;

        [[nodiscard]] double compute(storage::IModel *) const noexcept override;
    };
} // namespace graphquery::database::analytic
