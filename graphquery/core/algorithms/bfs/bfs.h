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
#include "db/utils/bitset.hpp"
#include "db/utils/sliding_queue.hpp"

namespace graphquery::database::analytic
{
    class CGraphAlgorithmBFS final : public IGraphAlgorithm
    {
      public:
        explicit CGraphAlgorithmBFS(std::string, const std::shared_ptr<logger::CLogSystem> &);
        ~CGraphAlgorithmBFS() override = default;

        [[nodiscard]] double compute(storage::IModel *) const noexcept override;

      private:
        static int64_t bu_step(const std::shared_ptr<std::vector<std::vector<int64_t>>> & inv_graph,
                               std::vector<int64_t> & parent,
                               const utils::CBitset<uint64_t> & front,
                               utils::CBitset<uint64_t> & next) noexcept;

        static int64_t td_step(storage::IModel * graph, std::vector<int64_t> & parent, utils::SlidingQueue<int64_t> & queue) noexcept;
        static std::vector<int64_t> init_parent(storage::IModel * graph, storage::Id_t sparse[], int64_t n_v, int64_t n_total_v) noexcept;
        static void queue_to_bitset(const utils::SlidingQueue<int64_t> & queue, utils::CBitset<uint64_t> & bm) noexcept;
        static void bitset_to_queue(storage::IModel * graph, const utils::CBitset<uint64_t> & bm, utils::SlidingQueue<int64_t> & queue) noexcept;
    };
} // namespace graphquery::database::analytic
