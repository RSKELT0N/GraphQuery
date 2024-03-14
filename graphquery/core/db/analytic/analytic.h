/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file analytic.h
 * \brief Analytic engine for loading and processing analytic
 *        algorithms against the currently loaded graph.
 ************************************************************/

#pragma once

#include "algorithm.h"
#include "dylib.hpp"
#include "db/utils/result.h"

#include <cassert>
#include <vector>
#include <future>
#include <unordered_map>

namespace graphquery::database::analytic
{
    class CAnalyticEngine
    {
      public:
        ~CAnalyticEngine() = default;
        explicit CAnalyticEngine(std::shared_ptr<storage::ILPGModel *> graph);

        void load_libraries(bool refresh = true) noexcept;
        void process_algorithm(std::string_view algorithm) noexcept;
        void process_algorithm(const std::vector<storage::ILPGModel::SEdge_t> & edges, std::string_view algorithm) noexcept;
        [[nodiscard]] std::shared_ptr<std::vector<utils::SResult<double>>> get_result_table() const noexcept;
        [[nodiscard]] const std::unordered_map<std::string, std::shared_ptr<IGraphAlgorithm *>> & get_algorithm_table() const noexcept;

      private:
        void insert_lib(std::string_view lib_path);

        std::shared_ptr<storage::ILPGModel *> m_graph;
        std::shared_ptr<std::vector<utils::SResult<double>>> m_results;
        std::unordered_map<std::string, std::shared_ptr<IGraphAlgorithm *>> m_algorithms;
        std::unordered_map<std::string, std::shared_ptr<dylib>> m_libs;

        static constexpr const char * LIB_FOLDER_PATH = "lib/algorithms";
    };
} // namespace graphquery::database::analytic
