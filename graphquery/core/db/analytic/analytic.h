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

#include <cassert>
#include <vector>
#include <functional>
#include <future>
#include <unordered_map>

namespace graphquery::database::analytic
{
    class CAnalyticEngine
    {
      public:
        ~CAnalyticEngine() = default;
        explicit CAnalyticEngine(std::shared_ptr<storage::ILPGModel *> graph);

        void process_algorithm(std::string_view algorithm) noexcept;

      private:
        struct SResult
        {
            SResult()  = default;
            ~SResult() = default;

            explicit SResult(const std::function<double()> & func): compute_function(func) { process_function(); }

            SResult(SResult && other) noexcept
            {
                this->compute_function = other.compute_function;
                this->resultant        = std::move(other.resultant);
                other.compute_function = nullptr;
                other.resultant        = {};
            }

            [[nodiscard]] inline bool processed() const noexcept { return resultant.valid(); }

            [[nodiscard]] inline double get_resultant() noexcept
            {
                assert(resultant.valid());
                return resultant.get();
            }

          private:
            inline void process_function()
            {
                assert(compute_function != nullptr);
                resultant = std::async(std::launch::async, compute_function);
            }

            std::future<double> resultant;
            std::function<double()> compute_function;
        };

        void load_libraries(bool refresh = true) noexcept;
        void insert_lib(std::string_view lib_path);

        std::vector<SResult> m_results;
        std::shared_ptr<storage::ILPGModel *> m_graph;
        std::unordered_map<std::string, std::unique_ptr<IGraphAlgorithm *>> m_algorithms;
        std::unordered_map<std::string, std::unique_ptr<dylib>> m_libs;

        static constexpr const char * LIB_FOLDER_PATH = "lib/algorithms";
    };
} // namespace graphquery::database::analytic
