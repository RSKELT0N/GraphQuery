#include "connected_components.h"

#include <algorithm>

graphquery::database::analytic::CGraphAlgorithmSSSP::CGraphAlgorithmSSSP(std::string name, const std::shared_ptr<logger::CLogSystem> & logsys): IGraphAlgorithm(std::move(name), logsys)
{
}

double
graphquery::database::analytic::CGraphAlgorithmSSSP::compute(storage::ILPGModel * graph_model) const noexcept
{
    const uint64_t n = graph_model->get_num_vertices();

    auto x      = new int[n];
    auto y      = new int[n];
    auto degree = new uint32_t[n];

    static constexpr int max_iter = 100;
    bool change                   = true;
    int iter                      = 0;

#pragma omp parallel for
    for (int i = 0; i < n; ++i)
    {
        x[i] = y[i] = i;
    }

    graph_model->calc_outdegree(degree);

    int lg = 0;
    for (int i = 1; i < n; ++i)
    {
        if (degree[i] > degree[lg])
            lg = i;
    }

    // Swap initial values
    x[0] = y[0] = lg;
    x[lg] = y[lg] = 0;

    std::unique_ptr<IRelax> CCrelax = std::make_unique<CRelaxCC>(x, y);

    while (iter < max_iter && change)
    {
        // 1. Assign same label to connected vertices
        graph_model->edgemap(CCrelax);
        // 2. Check changes and copy data over for new pass
        change = false;
        for (int i = 0; i < n; ++i)
        {
            if (x[i] != y[i])
            {
                x[i]   = y[i];
                change = true;
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
        if (x[i] == 0)
            x[i] = lg;
        else if (x[i] == lg)
            x[i] = 0;
        if (x[i] == i)
            remap[i] = ncc++;
    }

    m_log_system->info(fmt::format("Number of components: {}", ncc));

    // 2. Calculate size of each component
    auto sizes = new int[n];
    for (int i = 0; i < n; ++i)
    {
        ++sizes[remap[x[i]]];
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
