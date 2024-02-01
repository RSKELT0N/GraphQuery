/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file bfs.h
 * \brief Breadth first search (direction optimising)
 *        analytic algorithm to find the shortest path between
 *        a src and dst vertex.
 ************************************************************/

#pragma once

#include "db/analytic/algorithm.h"

namespace graphquery::database::analytic
{
    class CGraphAlgorithmBFS final : public IGraphAlgorithm
    {
      public:
        class CRelaxBFS final : public graphquery::database::analytic::IRelax
        {
          public:
            CRelaxBFS() {}

            ~CRelaxBFS()() override = default;

            void relax(const uint64_t src, const uint64_t dst) noexcept override {}
        };

        explicit CGraphAlgorithmBFS(std::string);
        ~CGraphAlgorithmBFS() override = default;

        [[nodiscard]] double compute(storage::ILPGModel *) noexcept override;

      private:
        std::shared_ptr<logger::CLogSystem> m_log_system;
    };
} // namespace graphquery::database::analytic
