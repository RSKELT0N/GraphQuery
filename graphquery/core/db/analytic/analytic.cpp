#include "analytic.h"

#include "db/system.h"
#include "db/utils/lib.h"

graphquery::database::analytic::CAnalyticEngine::
CAnalyticEngine(std::shared_ptr<storage::ILPGModel *> graph)
{
    this->m_graph      = std::move(graph);
    this->m_results    = std::make_shared<std::vector<utils::SResult<double>>>();
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

std::shared_ptr<std::vector<graphquery::database::utils::SResult<double>>>
graphquery::database::analytic::CAnalyticEngine::get_result_table() const noexcept
{
    return this->m_results;
}

const std::unordered_map<std::string, std::unique_ptr<graphquery::database::analytic::IGraphAlgorithm *>> &
graphquery::database::analytic::CAnalyticEngine::get_algorithm_table() const noexcept
{
    return this->m_algorithms;
}

void
graphquery::database::analytic::CAnalyticEngine::process_algorithm(std::string_view algorithm) noexcept
{
    if (!m_algorithms.contains(algorithm.data()))
        return;

    const auto & lib = m_algorithms.at(algorithm.data());
    m_results->emplace_back(algorithm,
                            [object_ptr = *lib.get(), capture0 = *m_graph, algorithm]
                            {
                                auto [res, elapsed] = utils::measure<double>(&IGraphAlgorithm::compute, object_ptr, capture0);
                                _log_system->info(fmt::format("Graph algorithm ({}) executed within {}s", algorithm, elapsed.count()));
                                return res;
                            });
}
