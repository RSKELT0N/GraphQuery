/************************************************************
* \author Ryan Skelton
 * \date 18/09/2023
 * \file result.h
 * \brief Header of commonly used generic used class for
 *        retreiving a future resultant.
 ************************************************************/

#pragma once

#include "fmt/format.h"

#include <cassert>
#include <chrono>
#include <functional>
#include <future>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace graphquery::database::utils
{
    template<typename T>
    struct SResult
    {
        SResult()  = default;
        ~SResult() = default;

        SResult(const std::string_view algorithm_name, const std::function<T()> & func): compute_function(func)
        {
            generate_name(algorithm_name);
            process_function();
        }

        [[nodiscard]] inline bool processed() const noexcept { return resultant.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
        [[nodiscard]] inline const std::string & get_name() const noexcept { return this->m_name; }

        [[nodiscard]] inline T get_resultant() const noexcept
        {
            assert(resultant.valid());
            return resultant.get();
        }

      private:
        inline void generate_name(const std::string_view algorithm_name) noexcept
        {
            std::stringstream ss;
            const auto curr_time   = std::chrono::system_clock::now();
            const auto curr_time_c = std::chrono::system_clock::to_time_t(curr_time);

            ss << std::put_time(std::localtime(&curr_time_c), "%Y-%m-%d %X");
            m_name = fmt::format("{} ({})", ss.str(), algorithm_name);
        }

        inline void process_function()
        {
            assert(compute_function != nullptr && !once);
            resultant = std::async(std::launch::async, compute_function);
            once      = true;
        }

        bool once = false;
        std::string m_name;
        std::shared_future<T> resultant;
        std::function<T()> compute_function;
    };
} // namespace graphquery::database::utils