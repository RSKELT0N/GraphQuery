#include "bfs.h"

#include "db/utils/bitset.hpp"
#include "db/utils/sliding_queue.hpp"

graphquery::database::analytic::CGraphAlgorithmBFS::
CGraphAlgorithmBFS(std::string name, const std::shared_ptr<logger::CLogSystem> & logsys): IGraphAlgorithm(std::move(name), logsys)
{
}

double
graphquery::database::analytic::CGraphAlgorithmBFS::compute(storage::ILPGModel * graph) const noexcept
{
    int64_t source                  = 7955;
    static constexpr int32_t alpha  = 15;
    static constexpr int32_t beta   = 18;
    const auto n_v                  = graph->get_num_vertices();
    const auto n_e                  = graph->get_num_edges();
    const auto n_total_v            = graph->get_total_num_vertices();
    const std::shared_ptr inv_graph = graph->make_inverse_graph();
    const auto sparse               = new int64_t[n_v];
    graph->calc_vertex_sparse_map(sparse);

    m_log_system->debug(fmt::format("Source: {}", source));

    if (source >= n_total_v)
        return 0.0;

    std::vector<int64_t> parent = init_parent(graph, sparse, n_v, n_total_v);
    parent[sparse[source]]      = source;

    utils::SlidingQueue<int64_t> queue(n_total_v);
    queue.push_back(sparse[source]);
    queue.slide_window();

    utils::CBitset<uint64_t> curr(n_total_v);
    curr.reset();

    utils::CBitset<uint64_t> front(n_total_v);
    front.reset();

    int64_t edges_to_check = n_e;
    int64_t scout_count    = graph->out_degree_by_offset(sparse[source]);

    while (!queue.empty())
    {
        if (scout_count > edges_to_check / alpha)
        {
            int64_t old_awake_count;
            queue_to_bitset(queue, front);

            int64_t awake_count = queue.size();
            queue.slide_window();
            do
            {
                old_awake_count = awake_count;
                awake_count     = bu_step(inv_graph, parent, front, curr);
                front.swap(curr);
            } while ((awake_count >= old_awake_count) || (awake_count > n_v / beta));
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
    for (int64_t n = 0; n < n_v; n++)
        if (parent[sparse[n]] < -1)
            parent[sparse[n]] = -1;

    double is_reachable_c = 0;

#pragma omp parallel for
    for(int64_t i = 0; i < n_v; i++)
    {
        if(parent[sparse[i]] != -1)
            is_reachable_c++;
    }

    return is_reachable_c;
}

std::vector<int64_t>
graphquery::database::analytic::CGraphAlgorithmBFS::init_parent(storage::ILPGModel * graph, int64_t sparse[], const int64_t n_v, const int64_t n_total_v) noexcept
{
    std::vector<int64_t> parent(n_total_v);

#pragma omp parallel for
    for (int64_t n = 0; n < n_v; n++)
        parent[sparse[n]] = graph->out_degree_by_offset(sparse[n]) != 0 ? -static_cast<int32_t>(graph->out_degree_by_offset(sparse[n])) : -1;

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
