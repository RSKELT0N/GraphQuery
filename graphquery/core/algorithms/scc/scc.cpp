#include "scc.h"

graphquery::database::analytic::CGraphAlgorithmSCC::CGraphAlgorithmSCC(std::string name, const std::shared_ptr<logger::CLogSystem> & logsys): IGraphAlgorithm(std::move(name), logsys)
{
}

double
graphquery::database::analytic::CGraphAlgorithmSCC::compute(storage::IModel * graph) const noexcept
{
    auto n_v_total = graph->get_total_num_vertices();
    auto n_total   = graph->get_num_vertices();

    auto sparse = new storage::Id_t[n_total];
    graph->calc_vertex_sparse_map(sparse);

    curr      = 0;
    ncc       = 0;
    represent = new int64_t[n_v_total];
    low       = new int64_t[n_v_total];
    disc      = new int64_t[n_v_total];
    instack   = new bool[n_v_total];

    for (int64_t i = 0; i < n_v_total; i++)
    {
        represent[i] = 0;
        disc[i]      = -1;
        low[i]       = 0;
        instack[i]   = false;
    }

    for (storage::Id_t i = 0; i < n_total; i++)
    {
        if (disc[sparse[i]] == -1)
            dfs(graph, sparse[i]);
    }

    delete[] represent;
    delete[] disc;
    delete[] low;
    delete[] instack;
    return static_cast<double>(ncc);
}

void
graphquery::database::analytic::CGraphAlgorithmSCC::dfs(storage::IModel * graph, const storage::Id_t v) const noexcept
{
    low[v] = disc[v] = utils::atomic_fetch_pre_inc(&curr);
    st.push_back(v);
    instack[v] = true;

    graph->src_edgemap(v,
                       [this, graph](const int64_t src, const int64_t dst) -> void
                       {
                           if (disc[dst] == -1)
                           {
                               dfs(graph, dst);
                               low[src] = std::min(low[src], low[dst]);
                           }
                           else if (instack[dst])
                               low[src] = std::min(low[src], disc[dst]);
                       });

    if (low[v] == disc[v])
    {
        utils::atomic_fetch_inc(&ncc);
        while (true)
        {
            const auto top = st.back();
            st.pop_back();
            instack[top]   = false;
            represent[top] = v;
            if (top == v)
                break;
        }
    }
}

extern "C"
{
    LIB_EXPORT void create_graph_algorithm(graphquery::database::analytic::IGraphAlgorithm ** graph_algorithm, const std::shared_ptr<graphquery::logger::CLogSystem> & logsys)
    {
        *graph_algorithm = new graphquery::database::analytic::CGraphAlgorithmSCC("SCC", logsys);
    }
}
