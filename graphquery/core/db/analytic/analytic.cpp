#include "analytic.h"

graphquery::database::analytic::CAnalyticEngine::CAnalyticEngine()
{
    this->m_results    = std::unique_ptr<std::vector<SResult>>();
    this->m_algorithms = std::vector<std::unique_ptr<IGraphAlgorithm>>();
    this->m_libs       = std::unique_ptr<std::set<dylib>>();
    load_libraries();
}

void
graphquery::database::analytic::CAnalyticEngine::load_libraries() noexcept
{
}

void
graphquery::database::analytic::CAnalyticEngine::refresh_libraries() noexcept
{
    assert(!m_libs->empty());
}

void
graphquery::database::analytic::CAnalyticEngine::process_algorithm(uint8_t idx) noexcept
{
}
