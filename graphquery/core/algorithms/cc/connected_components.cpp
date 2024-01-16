#include "connected_components.h"

#include <algorithm>

namespace
{
    class CRelaxCC final : public graphquery::database::analytic::IRelax
    {
      public:
        CRelaxCC(std::shared_ptr<int[]> _x, std::shared_ptr<int[]> _y)
        {
            this->x = std::move(_x);
            this->y = std::move(_y);
        }

        ~CRelaxCC() override = default;

        void relax(const uint64_t src, const uint64_t dst) noexcept override { y[dst] = std::min(y[dst], y[src]); }

        std::shared_ptr<int[]> x;
        std::shared_ptr<int[]> y;
    };
} // namespace

graphquery::database::analytic::CGraphAlgorithmCC::CGraphAlgorithmCC(std::string name): IGraphAlgorithm(std::move(name))
{
    m_log_system = logger::CLogSystem::get_instance();
}

double
graphquery::database::analytic::CGraphAlgorithmCC::compute(storage::ILPGModel * graph_model) noexcept
{
    const uint64_t n = graph_model->get_num_vertices();

    auto x      = std::make_shared<int[]>(n);
    auto y      = std::make_shared<int[]>(n);
    auto degree = std::make_shared<uint32_t[]>(n);

    static constexpr int max_iter = 100;
    bool change                   = true;
    int iter                      = 0;

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
    auto remap = std::make_shared<int[]>(n);

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
    auto sizes = std::make_shared<int[]>(n);
    for (int i = 0; i < n; ++i)
    {
        ++sizes[remap[x[i]]];
    }

    m_log_system->info(fmt::format("ConnectedComponents: {} components", ncc));

    return ncc;
}

extern "C"
{
    LIB_EXPORT void create_graph_algorithm(graphquery::database::analytic::IGraphAlgorithm ** graph_algorithm)
    {
        *graph_algorithm = new graphquery::database::analytic::CGraphAlgorithmCC("Connected Components");
    }
}
