#include "pagerank.h"

#include "db/analytic/relax.h"

#include <utility>

graphquery::database::analytic::CGraphAlgorithmPageRank::CGraphAlgorithmPageRank(std::string name, const std::shared_ptr<logger::CLogSystem> & logsys): IGraphAlgorithm(std::move(name), logsys)
{
}

double
graphquery::database::analytic::CGraphAlgorithmPageRank::compute(storage::IModel * graph_model) const noexcept
{
    const uint64_t n      = graph_model->get_num_vertices();
    const int64_t n_total = graph_model->get_total_num_vertices();

    auto x = new double[n_total];
    auto v = new double[n_total];
    auto y = new double[n_total];

    auto sparse = new storage::Id_t[n];

    graph_model->calc_vertex_sparse_map(sparse);

    static constexpr double d     = 0.85;
    static constexpr double tol   = 1e-7;
    static constexpr int max_iter = 100;
    double delta                  = 2;
    int iter                      = 0;

#pragma omp parallel for
    for (int i = 0; i < n; ++i)
    {
        x[sparse[i]] = v[sparse[i]] = 1.0 / static_cast<double>(n);
        y[sparse[i]]                = 0;
    }

    auto outdeg = new uint32_t[n_total];
    graph_model->calc_outdegree(outdeg);

    std::unique_ptr<IRelax> PRrelax = std::make_unique<CRelaxPR>(d, outdeg, x, y);

    while (iter < max_iter && delta > tol)
    {
        // Power iteration step.
        // 1. Transfering weight over out-going links (summation part)
        graph_model->edgemap(PRrelax);
        // 2. Constants (1-d)v[i] added in separately.
        double w = 1.0 - sum(y, sparse, n); // ensure y[] will sum to 1

#pragma omp parallel for
        for (int i = 0; i < n; ++i)
            y[sparse[i]] += w * v[sparse[i]];

        // Calculate residual error
        delta = norm_diff(x, y, sparse, n);
        iter++;

        // Rescale to unit length and swap x[] and y[]
        w = 1.0 / sum(y, sparse, n);

#pragma omp parallel for
        for (int i = 0; i < n; ++i)
        {
            x[sparse[i]] = y[sparse[i]] * w;
            y[sparse[i]] = 0.;
        }

        m_log_system->info(fmt::format("iteration {}: residual error= {} xnorm= {}", iter, delta, sum(x, sparse, n)));
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
graphquery::database::analytic::CGraphAlgorithmPageRank::sum(const double * vals, const storage::Id_t sparse[], const uint64_t size) noexcept
{
    double d   = 0.0F;
    double err = 0.0F;
#pragma omp parallel for firstprivate(err) reduction(+ : d)
    for (int i = 0; i < size; ++i)
    {
        const double tmp = d;
        double y         = vals[sparse[i]] + err;
        d                = tmp + y;
        err              = tmp - d;
        err += y;
    }
    return d;
}

double
graphquery::database::analytic::CGraphAlgorithmPageRank::norm_diff(const double val[],
                                                                   const double _val[],
                                                                   storage::Id_t sparse[],
                                                                   const uint64_t size) noexcept
{
    double d   = 0.0F;
    double err = 0.0F;
#pragma omp parallel for firstprivate(err) reduction(+ : d)
    for (int i = 0; i < size; ++i)
    {
        const double tmp = d;
        const double y   = fabs(_val[sparse[i]] - val[sparse[i]]) + err;
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