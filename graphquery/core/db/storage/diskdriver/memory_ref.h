/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file memory_ref.h
 * \brief Header describing a struct to interact with memory mapped
 *        files safely with reference counting.
 ************************************************************/

#pragma once

#include "db/utils/atomic_intrinsics.h"
#include "fmt/format.h"

#include <cstdint>
#include <shared_mutex>

namespace graphquery::database::storage
{
    template<typename T, bool write = false>
    struct SRef_t
    {
        inline SRef_t() = default;
        inline SRef_t(T * _t, std::shared_mutex * writer, uint8_t * _write, uint8_t * read): ref(_t), writer_lock(writer), write_l(_write), read_l(read) {}

        inline ~SRef_t()
        {
            if (writer_lock != nullptr)
            {
                if constexpr (!write)
                {
                    writer_lock->unlock_shared();
                    (*write_l)--;
                }

                if constexpr (write)
                {
                    writer_lock->unlock();
                    (*read_l)--;
                }
                writer_lock = nullptr;
            }
        }

        SRef_t(const SRef_t & cpy)             = delete;
        SRef_t & operator=(const SRef_t & cpy) = delete;

        SRef_t(SRef_t && cpy) noexcept: ref(cpy.ref), writer_lock(cpy.writer_lock), write_l(cpy.write_l), read_l(cpy.read_l)
        {
            cpy.ref         = nullptr;
            cpy.writer_lock = nullptr;
        }

        SRef_t & operator=(SRef_t && cpy) noexcept
        {
            if (this != &cpy)
            {
                ref             = cpy.ref;
                writer_lock     = cpy.writer_lock;
                write_l         = cpy.write_l;
                read_l          = cpy.read_l;
                cpy.ref         = nullptr;
                cpy.writer_lock = nullptr;
                cpy.write_l     = nullptr;
                cpy.read_l      = nullptr;
            }
            return *this;
        }

        inline void enter() const noexcept
        {
            if constexpr (!write)
            {
                writer_lock->lock_shared();
                (*read_l)++;
            }

            if constexpr (write)
            {
                writer_lock->lock();
                (*write_l)++;
            }
        }

        T * operator->() { return ref; }
        T * operator++(int) { return ref++; }
        T * operator+(uint64_t i) { return ref + i; }
        T * operator--(int) { return ref--; }
        T * operator--() { return --ref; }
        T * operator++() { return ++ref; }
        T operator*() { return *ref; }

        T * ref                         = nullptr;
        std::shared_mutex * writer_lock = nullptr;
        uint8_t * write_l               = nullptr;
        uint8_t * read_l                = nullptr;
    };
}; // namespace graphquery::database::storage