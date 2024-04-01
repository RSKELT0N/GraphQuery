/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file memory_ref.h
 * \brief Header describing a struct to interact with memory mapped
 *        files safely with reference counting.
 ************************************************************/

#pragma once

#include "db/utils/spinlock.h"
#include "db/utils/atomic_intrinsics.h"
#include "fmt/format.h"

#include <cstdint>

namespace graphquery::database::storage
{
    template<typename T, bool write = false>
    struct SRef_t
    {
        inline SRef_t() = default;
        inline SRef_t(T * _t, CSpinlock * writer, CSpinlock * reader, uint8_t * read_c): ref(_t), writer_lock(writer), reader_lock(reader), reader_c(read_c) {}

        inline ~SRef_t()
        {
            if (writer_lock != nullptr && reader_c != nullptr && reader_lock != nullptr)
            {
                if constexpr (write)
                    writer_lock->unlock();

                if constexpr (!write)
                {
                    if (utils::atomic_fetch_pre_dec(reader_c) == 0)
                        writer_lock->unlock();
                }
            }
            reader_c    = nullptr;
            reader_lock = nullptr;
            writer_lock = nullptr;
        }

        SRef_t(const SRef_t & cpy)
        {
            ref         = cpy.ref;
            writer_lock = cpy.writer_lock;
            reader_lock = cpy.reader_lock;
            reader_c    = cpy.reader_c;
            enter();
        }

        SRef_t & operator=(const SRef_t & cpy)
        {
            if (this != &cpy)
            {
                ref         = cpy.ref;
                writer_lock = cpy.writer_lock;
                reader_lock = cpy.reader_lock;
                reader_c    = cpy.reader_c;
                enter();
            }
            return *this;
        }

        SRef_t(SRef_t && cpy) noexcept: ref(cpy.ref), writer_lock(cpy.writer_lock), reader_lock(cpy.reader_lock), reader_c(cpy.reader_c)
        {
            cpy.ref         = nullptr;
            cpy.writer_lock = nullptr;
            cpy.reader_lock = nullptr;
            cpy.reader_c    = nullptr;
        }

        SRef_t & operator=(SRef_t && cpy) noexcept
        {
            if (this != &cpy)
            {
                ref             = cpy.ref;
                writer_lock     = cpy.writer_lock;
                reader_lock     = cpy.reader_lock;
                reader_c        = cpy.reader_c;
                cpy.ref         = nullptr;
                cpy.writer_lock = nullptr;
                cpy.reader_lock = nullptr;
                cpy.reader_c    = nullptr;
            }
            return *this;
        }

        inline void enter() const noexcept
        {
            if constexpr (write)
                writer_lock->lock();

            if constexpr (!write)
            {
                reader_lock->lock();
                if (utils::atomic_fetch_pre_inc(reader_c) == 1)
                    writer_lock->lock();
                reader_lock->unlock();
            }
        }

        inline T * operator->() { return ref; }
        inline T * operator++(int) { return ref++; }
        inline T * operator+(uint64_t i) { return ref + i; }
        inline T * operator--(int) { return ref--; }
        inline T * operator--() { return --ref; }
        inline T * operator++() { return ++ref; }
        inline T operator*() { return *ref; }

        T * ref                 = nullptr;
        CSpinlock * writer_lock = nullptr;
        CSpinlock * reader_lock = nullptr;
        uint8_t * reader_c      = nullptr;
    };
}; // namespace graphquery::database::storage
