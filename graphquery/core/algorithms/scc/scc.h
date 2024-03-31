/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file scc.h
 * \brief Strong Connected components is a analytic algorithm to calculate
 *        the distinct number of isolated vertices. Also known as
 *        disjoint set, to determine the subset count of the vertices.
 ************************************************************/

#pragma once

#include "db/analytic/algorithm.h"

#include <stack>

namespace graphquery::database::analytic
{
    class CGraphAlgorithmSCC final : public IGraphAlgorithm
    {
      public:
        explicit CGraphAlgorithmSCC(std::string, const std::shared_ptr<logger::CLogSystem> &);
        ~CGraphAlgorithmSCC() override = default;

        [[nodiscard]] double compute(storage::IModel *) const noexcept override;

      private:
        void dfs(storage::IModel * graph, storage::Id_t v) const noexcept;

        mutable int32_t ncc;
        mutable int32_t curr;
        mutable int64_t * represent;
        mutable int64_t * disc;
        mutable int64_t * low;
        mutable bool * instack;
        mutable std::vector<int64_t> st;
    };
} // namespace graphquery::database::analytic
