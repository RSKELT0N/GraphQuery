#include "pagerank.h"

#include "db/analytic/relax.h"

#include <utility>

graphquery::database::analytic::CGraphAlgorithmPageRank::
CGraphAlgorithmPageRank(std::string name, const std::shared_ptr<logger::CLogSystem> & logsys): IGraphAlgorithm(std::move(name), logsys) {}

double
graphquery::database::analytic::CGraphAlgorithmPageRank::compute(storage::ILPGModel * graph_model) const noexcept
{
    const uint64_t n = graph_model->get_num_vertices();

    auto x = new double[n];
    auto v = new double[n];
    auto y = new double[n];

    static constexpr double d     = 0.85;
    static constexpr double tol   = 1e-7;
    static constexpr int max_iter = 100;
    double delta                  = 2;
    int iter                      = 0;

#pragma omp parallel for
    for (int i = 0; i < n; ++i)
    {
        x[i] = v[i] = 1.0 / static_cast<double>(n);
        y[i]        = 0;
    }

    auto outdeg = new uint32_t[n];
    graph_model->calc_outdegree(outdeg);

    std::unique_ptr<IRelax> PRrelax = std::make_unique<CRelaxPR>(d, outdeg, x, y);

    while (iter < max_iter && delta > tol)
    {
        // Power iteration step.
        // 1. Transfering weight over out-going links (summation part)
        graph_model->edgemap(PRrelax);
        // 2. Constants (1-d)v[i] added in separately.
        double w = 1.0 - sum(y, n); // ensure y[] will sum to 1

#pragma omp parallel for
        for (int i = 0; i < n; ++i)
            y[i] += w * v[i];

        // Calculate residual error
        delta = norm_diff(x, y, n);
        iter++;

        // Rescale to unit length and swap x[] and y[]
        w = 1.0 / sum(y, n);

#pragma omp parallel for
        for (int i = 0; i < n; ++i)
        {
            x[i] = y[i] * w;
            y[i] = 0.;
        }

        m_log_system->info(fmt::format("iteration {}: residual error= {} xnorm= {}", iter, delta, sum(x, n)));
    }

    if (delta > tol)
        m_log_system->warning("Error: solution has not converged");

    delete[] x;
    delete[] v;
    delete[] y;
    delete[] outdeg;

    return delta;
}

double
graphquery::database::analytic::CGraphAlgorithmPageRank::sum(const double * vals, const uint64_t size) noexcept
{
    double d   = 0.0F;
    double err = 0.0F;
    for (int i = 0; i < size; ++i)
    {
        const double tmp = d;
        double y         = vals[i] + err;
        d                = tmp + y;
        err              = tmp - d;
        err += y;
    }
    return d;
}

double
graphquery::database::analytic::CGraphAlgorithmPageRank::norm_diff(const double val[],
                                                                   const double _val[],
                                                                   const uint64_t size) noexcept
{
    double d   = 0.0F;
    double err = 0.0F;
    for (int i = 0; i < size; ++i)
    {
        const double tmp = d;
        const double y   = fabs(_val[i] - val[i]) + err;
        d                = tmp + y;
        err              = tmp - d;
        err += y;
    }
    return d;
}

extern "C"
{
    LIB_EXPORT void create_graph_algorithm(graphquery::database::analytic::IGraphAlgorithm ** graph_algorithm, const std::shared_ptr<graphquery::logger::CLogSystem> & logsys)
    {
        *graph_algorithm = new graphquery::database::analytic::CGraphAlgorithmPageRank("PageRank", logsys);
    }
}
