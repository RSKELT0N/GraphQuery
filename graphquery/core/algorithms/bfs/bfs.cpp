#include "bfs.h"

graphquery::database::analytic::CGraphAlgorithmBFS::
CGraphAlgorithmBFS(std::string name): IGraphAlgorithm(std::move(name))
{
    this->m_log_system = logger::CLogSystem::get_instance();
}

double 
graphquery::database::analytic::CGraphAlgorithmBFS::compute(storage::ILPGModel *) noexcept
{
    return 0.0;
}

extern "C"
{
    LIB_EXPORT void create_graph_algorithm(graphquery::database::analytic::IGraphAlgorithm ** graph_algorithm)
    {
        *graph_algorithm = new graphquery::database::analytic::CGraphAlgorithmBFS("BFS");
    }
}
