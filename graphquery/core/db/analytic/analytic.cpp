#include "analytic.h"

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

    try
    {
        const auto & files = std::filesystem::directory_iterator(fmt::format("{}/{}", PROJECT_ROOT, LIB_FOLDER_PATH));

        for (const auto & file : files)
            insert_lib(file.path().c_str());
    }
    catch (const std::exception & e)
    {
        _log_system->error(fmt::format("Issue loading libraries (from lib) into analytic engine, Error: {}", e.what()));
    }
}

void
graphquery::database::analytic::CAnalyticEngine::insert_lib(const std::string_view lib_path)
{
    auto ptr = std::make_unique<IGraphAlgorithm *>();
    auto lib = std::make_unique<dylib>(dylib(lib_path));

    lib->get_function<void(IGraphAlgorithm **, const std::shared_ptr<logger::CLogSystem> &)>("create_graph_algorithm")(ptr.get(), _log_system);

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
