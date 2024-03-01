/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file thread_pool.hpp
 * \brief Header file to provide functionality for threading
 *        with holding a pool of threads.
 ************************************************************/

#pragma once

#include <algorithm>
#include <thread>
#include <array>
#include <functional>

namespace graphquery::database::utils
{
    template<size_t N = 8>
    class CThreadPool final
    {
      public:
        CThreadPool() { m_thread_pool = std::array<std::thread, N>(); }

        ~CThreadPool()                                   = default;
        CThreadPool(const CThreadPool &)                 = delete;
        CThreadPool(CThreadPool &&) noexcept             = delete;
        CThreadPool & operator=(const CThreadPool &)     = delete;
        CThreadPool & operator=(CThreadPool &&) noexcept = delete;

        template<typename T>
        void attach(T func)
        {
            std::for_each(m_thread_pool.begin(), m_thread_pool.end(), [&func](std::thread & _t) -> void { _t = std::thread(func); });
        }

        void join() noexcept
        {
            std::for_each(m_thread_pool.begin(), m_thread_pool.end(), [](std::thread & _t) -> void { _t.join(); });
        }

        void parallel_for([[maybe_unused]] const uint64_t elem_c, [[maybe_unused]] const std::function<void(uint64_t i)> & func) noexcept {}

      private:
        std::array<std::thread, N> m_thread_pool;
    };
}; // namespace graphquery::database::utils
