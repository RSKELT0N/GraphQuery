/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file memory_ref.h
 * \brief Header describing a struct to interact with memory mapped
 *        files safely with reference counting.
 ************************************************************/

#pragma once

#include "db/utils/atomic_intrinsics.h"

#include <mutex>
#include <cstdint>

#include "db/utils/spinlock.h"

namespace graphquery::database::storage
{
    template<typename T, bool write = false>
    struct SRef_t
    {
        inline SRef_t() = default;

        inline SRef_t(T * _t, uint32_t * readers, std::mutex * writer):
            ref(_t), r_c(readers), writer_lock(writer)
        {
        }

        inline ~SRef_t()
        {
            if (r_c != nullptr && writer_lock != nullptr)
            {
                if constexpr (write)
                    writer_lock->unlock();

                if constexpr (!write)
                    if (utils::atomic_fetch_pre_dec(&(*r_c)) == 0)
                        writer_lock->unlock();

                r_c         = nullptr;
                writer_lock = nullptr;
            }
        }

        SRef_t(const SRef_t & cpy)
        {
            ref         = cpy.ref;
            r_c         = cpy.r_c;
            writer_lock = cpy.writer_lock;
            enter();
        }

        SRef_t & operator=(const SRef_t & cpy)
        {
            if (this != &cpy)
            {
                ref         = cpy.ref;
                r_c         = cpy.r_c;
                writer_lock = cpy.writer_lock;
                enter();
            }
            return *this;
        }

        SRef_t(SRef_t && cpy) noexcept:
            ref(cpy.ref), r_c(cpy.r_c), writer_lock(cpy.writer_lock)
        {
            cpy.ref         = nullptr;
            cpy.r_c         = nullptr;
            cpy.writer_lock = nullptr;
        }

        SRef_t & operator=(SRef_t && cpy) noexcept
        {
            if (this != &cpy)
            {
                ref             = cpy.ref;
                r_c             = cpy.r_c;
                writer_lock     = cpy.writer_lock;
                cpy.ref         = nullptr;
                cpy.r_c         = nullptr;
                cpy.writer_lock = nullptr;
            }
            return *this;
        }

        inline void enter() const noexcept
        {
            if constexpr (!write)
                utils::atomic_fetch_pre_inc(&(*r_c));
        }

        T * operator->() { return ref; }
        T * operator++(int) { return ref++; }
        T * operator+(uint64_t i) { return ref + i; }
        T * operator--(int) { return ref--; }
        T * operator--() { return --ref; }
        T * operator++() { return ++ref; }
        T operator*() { return *ref; }

        T * ref        = nullptr;
        uint32_t * r_c = nullptr;
        std::mutex * writer_lock = nullptr;
    };
}; // namespace graphquery::database::storage