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
    template<size_t N = std::thread::hardware_concurrency()>
    class CThreadPool final
    {
      public:
        CThreadPool() { m_thread_pool = std::array<std::thread, N>(); }

        ~CThreadPool()                                   = delete;
        CThreadPool(const CThreadPool &)                 = delete;
        CThreadPool(CThreadPool &&) noexcept             = delete;
        CThreadPool & operator=(const CThreadPool &)     = delete;
        CThreadPool & operator=(CThreadPool &&) noexcept = delete;

        void process_chunk(uint32_t from, uint32_t to) const noexcept {}

        void parallel_for(const uint32_t elem_c, const std::function<void()> & func) noexcept
        {
            this->attach(func);

            uint32_t chunk_size = elem_c / N;
            uint32_t from;
            uint32_t to;

            func(from, to);

            this->join();
        }

        template<typename T>
        void attach(const std::function<T> & func) noexcept
        {
            std::for_each(m_thread_pool.begin(), m_thread_pool.end(), [&func](std::thread & _t) -> void { _t = std::thread(func); });
        }

        void join() const noexcept
        {
            std::for_each(m_thread_pool.begin(), m_thread_pool.end(), [](std::thread & _t) -> void { _t.join(); });
        }

      private:
        std::array<std::thread, N> m_thread_pool;
    };
}; // namespace graphquery::database::utils
