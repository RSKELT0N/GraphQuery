/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file sssp.h
 *  \brief Single shortest short path (delta stepping)
 *         analytic algorithm for finding the distances between
 *         a src vertex to all vertices in the graph.
 ************************************************************/

#pragma once

#include "db/analytic/algorithm.h"

namespace graphquery::database::analytic
{
    class CGraphAlgorithmSSSP final : public IGraphAlgorithm
    {
      public:
        class CRelaxSSSP final : public graphquery::database::analytic::IRelax
        {
          public:
            CRelaxSSSP() {}

            ~CRelaxSSSP()() override = default;

            void relax(const uint64_t src, const uint64_t dst) noexcept override {}
        };

        explicit CGraphAlgorithmSSSP(std::string);
        ~CGraphAlgorithmSSSP() override = default;

        [[nodiscard]] double compute(storage::ILPGModel *) noexcept override;

      private:
        std::shared_ptr<logger::CLogSystem> m_log_system;
    };
} // namespace graphquery::database::analytic
