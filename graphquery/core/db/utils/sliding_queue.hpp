/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file sliding_queue.hpp
 * \brief Header file to provide functionality for sliding queue
 *        implementation for bfs algorithm (direction optimising)
 *
 *        Author: Scott Beamer
 *         Double-buffered queue so appends aren't seen until SlideWindow() called
 *         - Use QueueBuffer when used in parallel to avoid false sharing by doing
 *           bulk appends from thread-local storage
 *************************************************************/

#pragma once

#include <algorithm>

#include "atomic_intrinsics.h"

namespace graphquery::database::utils
{
    template<typename T>
    class QueueBuffer;

    template<typename T>
    class SlidingQueue
    {
        T * shared;
        size_t shared_in;
        size_t shared_out_start;
        size_t shared_out_end;
        friend class QueueBuffer<T>;

      public:
        explicit SlidingQueue(size_t shared_size)
        {
            shared = new T[shared_size];
            reset();
        }

        ~SlidingQueue() { delete[] shared; }

        void push_back(T to_add) { shared[shared_in++] = to_add; }

        bool empty() const { return shared_out_start == shared_out_end; }

        void reset()
        {
            shared_out_start = 0;
            shared_out_end   = 0;
            shared_in        = 0;
        }

        void slide_window()
        {
            shared_out_start = shared_out_end;
            shared_out_end   = shared_in;
        }

        typedef T * iterator;

        iterator begin() const { return shared + shared_out_start; }

        iterator end() const { return shared + shared_out_end; }

        size_t size() const { return end() - begin(); }
    };

    template<typename T>
    class QueueBuffer
    {
        size_t in;
        T * local_queue;
        SlidingQueue<T> & sq;
        const size_t local_size;

      public:
        explicit QueueBuffer(SlidingQueue<T> & master, size_t given_size = 16384): sq(master), local_size(given_size)
        {
            in          = 0;
            local_queue = new T[local_size];
        }

        ~QueueBuffer() { delete[] local_queue; }

        void push_back(T to_add)
        {
            if (in == local_size)
                flush();
            local_queue[in++] = to_add;
        }

        void flush()
        {
            T * shared_queue  = sq.shared;
            size_t copy_start = utils::atomic_fetch_add(&sq.shared_in, in);
            std::copy(local_queue, local_queue + in, shared_queue + copy_start);
            in = 0;
        }
    };
} // namespace graphquery::database::utils