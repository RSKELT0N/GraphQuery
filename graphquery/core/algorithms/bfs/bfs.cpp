#include "bfs.h"

#include "db/utils/bitset.hpp"
#include "db/utils/sliding_queue.hpp"

graphquery::database::analytic::CGraphAlgorithmBFS::CGraphAlgorithmBFS(std::string name, const std::shared_ptr<logger::CLogSystem> & logsys): IGraphAlgorithm(std::move(name), logsys)
{
}

double
graphquery::database::analytic::CGraphAlgorithmBFS::compute(storage::ILPGModel * graph) const noexcept
{
    int64_t source                  = 7955;
    static constexpr int32_t alpha  = 15;
    static constexpr int32_t beta   = 18;
    const auto num_vertices         = graph->get_num_vertices();
    const auto num_edges            = graph->get_num_edges();
    const std::shared_ptr inv_graph = graph->make_inverse_graph();

    m_log_system->debug(fmt::format("Source: {}", source));

    if (source >= num_vertices)
        return 0.0;

    std::vector<int64_t> parent = init_parent(graph);
    parent[source]              = source;

    utils::SlidingQueue<int64_t> queue(num_vertices);
    queue.push_back(source);
    queue.slide_window();

    utils::CBitset<uint64_t> curr(num_vertices);
    curr.reset();

    utils::CBitset<uint64_t> front(num_vertices);
    front.reset();

    int64_t edges_to_check = num_edges;
    int64_t scout_count    = graph->out_degree_by_offset(source);

    while (!queue.empty())
    {
        if (scout_count > edges_to_check / alpha)
        {
            int64_t awake_count, old_awake_count;
            queue_to_bitset(queue, front);

            awake_count = queue.size();
            queue.slide_window();
            do
            {
                old_awake_count = awake_count;
                awake_count     = bu_step(inv_graph, parent, front, curr);
                front.swap(curr);
            } while ((awake_count >= old_awake_count) || (awake_count > num_vertices / beta));
            bitset_to_queue(graph, front, queue);
            scout_count = 1;
        }
        else
        {
            edges_to_check -= scout_count;
            scout_count = td_step(graph, parent, queue);
            queue.slide_window();
        }
    }

#pragma omp parallel for
    for (int64_t n = 0; n < num_vertices; n++)
        if (parent[n] < -1)
            parent[n] = -1;

    double is_reachable_c = 0;
    fmt::print("ddd");
    fmt::print("ddd");
    fmt::print("ddd");
    fmt::print("ddd");

    std::for_each(parent.begin(),
                  parent.end(),
                  [&is_reachable_c](const auto dist) -> void
                  {
                      if (dist != -1)
                          is_reachable_c++;
                  });

     return is_reachable_c;
}

std::vector<int64_t>
graphquery::database::analytic::CGraphAlgorithmBFS::init_parent(storage::ILPGModel * graph) noexcept
{
    const int64_t vertex_c = graph->get_num_vertices();
    std::vector<int64_t> parent(graph->get_num_vertices());

#pragma omp parallel for
    for (int64_t n = 0; n < vertex_c; n++)
        parent[n] = graph->out_degree_by_offset(n) != 0 ? -static_cast<int32_t>(graph->out_degree_by_offset(n)) : -1;

    return parent;
}

void
graphquery::database::analytic::CGraphAlgorithmBFS::queue_to_bitset(const utils::SlidingQueue<int64_t> & queue, utils::CBitset<uint64_t> & bm) noexcept
{
#pragma omp parallel for
    for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++)
    {
        int64_t u = *q_iter;
        bm.set(u);
    }
}

void
graphquery::database::analytic::CGraphAlgorithmBFS::bitset_to_queue(storage::ILPGModel * graph, const utils::CBitset<uint64_t> & bm, utils::SlidingQueue<int64_t> & queue) noexcept
{
#pragma omp parallel
    {
        utils::QueueBuffer<int64_t> lqueue(queue);
#pragma omp for nowait
        for (int64_t n = 0; n < graph->get_num_vertices(); n++)
            if (bm.get(n))
                lqueue.push_back(n);
        lqueue.flush();
    }
    queue.slide_window();
}

int64_t
graphquery::database::analytic::CGraphAlgorithmBFS::bu_step(const std::shared_ptr<std::vector<std::vector<int64_t>>> & inv_graph,
                                                            std::vector<int64_t> & parent,
                                                            const utils::CBitset<uint64_t> & front,
                                                            utils::CBitset<uint64_t> & next) noexcept
{
    int64_t awake_count = 0;
    next.reset();
#pragma omp parallel for reduction(+ : awake_count) schedule(dynamic, 1024)
    for (int64_t dst = 0; dst < inv_graph->size(); dst++)
    {
        if (parent[dst] < 0)
        {
            for (int64_t src : (*inv_graph)[dst])
            {
                if (front.get(src))
                {
                    parent[dst] = src;
                    awake_count++;
                    next.set(dst);
                    break;
                }
            }
        }
    }
    return awake_count;
}

int64_t
graphquery::database::analytic::CGraphAlgorithmBFS::td_step(storage::ILPGModel * graph, std::vector<int64_t> & parent, utils::SlidingQueue<int64_t> & queue) noexcept
{
    int64_t scout_count = 0;
#pragma omp parallel
    {
        utils::QueueBuffer lqueue(queue);

        const auto relax = [&parent, &lqueue, &scout_count](int64_t src, const int64_t dst) -> void
        {
            int64_t curr_val = parent[dst];
            if (curr_val < 0)
            {
                if (utils::atomic_fetch_cas(&parent[dst], curr_val, src))
                {
                    lqueue.push_back(dst);
                    scout_count += -curr_val;
                }
            }
        };

#pragma omp for reduction(+ : scout_count) nowait
        for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++)
        {
            int64_t u = *q_iter;
            graph->src_edgemap(u, relax);
        }
        lqueue.flush();
    }
    return scout_count;
}

extern "C"
{
    LIB_EXPORT void create_graph_algorithm(graphquery::database::analytic::IGraphAlgorithm ** graph_algorithm, const std::shared_ptr<graphquery::logger::CLogSystem> & logsys)
    {
        *graph_algorithm = new graphquery::database::analytic::CGraphAlgorithmBFS("BFS", logsys);
    }
}
