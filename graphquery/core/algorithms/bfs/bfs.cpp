#include "bfs.h"

graphquery::database::analytic::CGraphAlgorithmBFS::
CGraphAlgorithmBFS(std::string name, const std::shared_ptr<logger::CLogSystem> & logsys): IGraphAlgorithm(std::move(name), logsys) {}

double 
graphquery::database::analytic::CGraphAlgorithmBFS::compute(storage::ILPGModel *) const noexcept
{
    return 0.0;
}

extern "C"
{
    LIB_EXPORT void create_graph_algorithm(graphquery::database::analytic::IGraphAlgorithm ** graph_algorithm, const std::shared_ptr<graphquery::logger::CLogSystem> & logsys)
    {
        *graph_algorithm = new graphquery::database::analytic::CGraphAlgorithmBFS("BFS", logsys);
    }
}
