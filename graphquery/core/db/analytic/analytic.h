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

        struct SResult
        {
            SResult()  = default;
            ~SResult() = default;

            SResult(const std::string_view algorithm_name, const std::function<double()> & func): compute_function(func)
            {
                generate_name(algorithm_name);
                process_function();
            }

            [[nodiscard]] inline bool processed() const noexcept { return resultant.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
            [[nodiscard]] inline const std::string & get_name() const noexcept { return this->m_name; }

            [[nodiscard]] inline double get_resultant() const noexcept
            {
                assert(resultant.valid());
                return resultant.get();
            }

          private:
            inline void generate_name(const std::string_view algorithm_name) noexcept
            {
                // ~ Get the current time of the log call.
                std::stringstream ss;
                const auto curr_time   = std::chrono::system_clock::now();
                const auto curr_time_c = std::chrono::system_clock::to_time_t(curr_time);

                ss << std::put_time(std::localtime(&curr_time_c), "%Y-%m-%d %X");
                m_name = fmt::format("{} ({})\n", ss.str(), algorithm_name);
            }

            inline void process_function()
            {
                assert(compute_function != nullptr && !once);
                resultant = std::async(std::launch::async, compute_function);
                once      = true;
            }

            bool once = false;
            std::string m_name;
            std::shared_future<double> resultant;
            std::function<double()> compute_function;
        };

        void load_libraries(bool refresh = true) noexcept;
        void process_algorithm(std::string_view algorithm) noexcept;
        std::shared_ptr<std::vector<SResult>> get_result_table() const noexcept;
        const std::unordered_map<std::string, std::unique_ptr<IGraphAlgorithm *>> & get_algorithm_table() const noexcept;

      private:
        void insert_lib(std::string_view lib_path);

        std::shared_ptr<storage::ILPGModel *> m_graph;
        std::shared_ptr<std::vector<SResult>> m_results;
        std::unordered_map<std::string, std::unique_ptr<IGraphAlgorithm *>> m_algorithms;
        std::unordered_map<std::string, std::unique_ptr<dylib>> m_libs;

        static constexpr const char * LIB_FOLDER_PATH = "lib/algorithms";
    };
} // namespace graphquery::database::analytic
