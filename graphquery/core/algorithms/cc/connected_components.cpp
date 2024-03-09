#include "connected_components.h"

#include <algorithm>

graphquery::database::analytic::CGraphAlgorithmSSSP::
CGraphAlgorithmSSSP(std::string name, const std::shared_ptr<logger::CLogSystem> & logsys): IGraphAlgorithm(std::move(name), logsys)
{
}

double
graphquery::database::analytic::CGraphAlgorithmSSSP::compute(storage::ILPGModel * graph_model) const noexcept
{
    const uint64_t n      = graph_model->get_num_vertices();
    const int64_t total_n = graph_model->get_total_num_vertices();

    auto x      = new int[total_n];
    auto y      = new int[total_n];
    auto degree = new uint32_t[total_n];
    auto sparse = new int64_t[n];

    graph_model->calc_vertex_sparse_map(sparse);

    static constexpr int max_iter = 100;
    bool change                   = true;
    int iter                      = 0;

#pragma omp parallel for
    for (int i = 0; i < n; ++i)
    {
        x[sparse[i]] = y[sparse[i]] = i;
    }

    graph_model->calc_outdegree(degree);

    int lg = 0;
    for (int i = 1; i < n; ++i)
    {
        if (degree[sparse[i]] > degree[sparse[lg]])
            lg = i;
    }

    // Swap initial values
    x[sparse[0]] = y[sparse[0]] = lg;
    x[sparse[lg]] = y[sparse[lg]] = 0;

    std::unique_ptr<IRelax> CCrelax = std::make_unique<CRelaxCC>(x, y);

    while (iter < max_iter && change)
    {
        // 1. Assign same label to connected vertices
        graph_model->edgemap(CCrelax);
        // 2. Check changes and copy data over for new pass
        change = false;
        for (int i = 0; i < n; ++i)
        {
            if (x[sparse[i]] != y[sparse[i]])
            {
                x[sparse[i]] = y[sparse[i]];
                change       = true;
            }
        }
        ++iter;
    }

    // Post-process the labels

    // 1. Count number of components
    //    and map component IDs to narrow domain
    int ncc    = 0;
    auto remap = new int[n];

    for (int i = 0; i < n; ++i)
    {
        // "unswap" values such that we can identify representatives
        if (x[sparse[i]] == 0)
            x[sparse[i]] = lg;
        else if (x[sparse[i]] == lg)
            x[sparse[i]] = 0;
        if (x[sparse[i]] == i)
            remap[sparse[i]] = ncc++;
    }

    m_log_system->info(fmt::format("Number of components: {}", ncc));

    // 2. Calculate size of each component
    auto sizes = new int[n];
    for (int i = 0; i < n; ++i)
    {
        ++sizes[remap[x[sparse[i]]]];
    }

    m_log_system->info(fmt::format("ConnectedComponents: {} components", ncc));

    delete[] x;
    delete[] y;
    delete[] degree;
    delete[] remap;
    delete[] sizes;
    return ncc;
}

extern "C"
{
    LIB_EXPORT void create_graph_algorithm(graphquery::database::analytic::IGraphAlgorithm ** graph_algorithm, const std::shared_ptr<graphquery::logger::CLogSystem> & logsys)
    {
        *graph_algorithm = new graphquery::database::analytic::CGraphAlgorithmSSSP("ConnectedComponents", logsys);
    }
}
