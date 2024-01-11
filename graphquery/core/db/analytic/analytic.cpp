#include "analytic.h"

#include <utility>

#include "db/system.h"

graphquery::database::analytic::CAnalyticEngine::CAnalyticEngine(std::shared_ptr<storage::ILPGModel *> graph)
{
    this->m_graph      = std::move(graph);
    this->m_results    = std::vector<SResult>();
    this->m_algorithms = std::unordered_map<std::string, std::unique_ptr<IGraphAlgorithm *>>();
    this->m_libs       = std::unordered_map<std::string, std::unique_ptr<dylib>>();
    load_libraries(false);
}

void
graphquery::database::analytic::CAnalyticEngine::load_libraries(const bool refresh) noexcept
{
    if (refresh)
        m_algorithms.clear();

    for (const auto & file : std::filesystem::directory_iterator(fmt::format("{}/{}", PROJECT_ROOT, LIB_FOLDER_PATH)))
    {
        try
        {
            insert_lib(file.path().c_str());
        }
        catch (std::runtime_error & e)
        {
            _log_system->error(fmt::format("Issue linking library and creating graph algorithm ({}) Error: {}", file.path().c_str(), e.what()));
        }
    }
}

void
graphquery::database::analytic::CAnalyticEngine::insert_lib(const std::string_view lib_path)
{
    auto ptr       = std::make_unique<IGraphAlgorithm *>();
    auto lib       = std::make_unique<dylib>(dylib(lib_path));

    lib->get_function<void(IGraphAlgorithm **)>("create_graph_algorithm")(ptr.get());

    m_libs.emplace((*ptr)->get_name(), std::move(lib));
    m_algorithms.emplace((*ptr)->get_name(), std::move(ptr));
}

void
graphquery::database::analytic::CAnalyticEngine::process_algorithm(const std::string_view algorithm) noexcept
{
    if (!m_algorithms.contains(algorithm.data()))
        return;

    const auto & lib = m_algorithms.at(algorithm.data());

    static auto lambda = [&lib, this]() -> double { return (*lib)->compute(*m_graph); };

    m_results.emplace_back(lambda);
}
