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
#include <cstdint>
#include <functional>
#include <future>
#include <set>

namespace graphquery::database::analytic
{
    class CAnalyticEngine
    {
      public:
        CAnalyticEngine();
        ~CAnalyticEngine() = default;

      private:
        struct SResult
        {
            ~SResult() = default;
            explicit SResult(const std::function<double()> & func): compute_function(func) { process_function(); }
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

        void load_libraries() noexcept;
        void refresh_libraries() noexcept;
        void process_algorithm(uint8_t idx) noexcept;

        std::unique_ptr<std::vector<SResult>> m_results;
        std::unique_ptr<std::set<dylib>> m_libs;
        std::vector<std::unique_ptr<IGraphAlgorithm>> m_algorithms;
    };
} // namespace graphquery::database::analytic
