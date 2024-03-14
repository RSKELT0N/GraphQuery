/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file memory_ref.h
 * \brief Header describing a struct to interact with memory mapped
 *        files safely with reference counting.
 ************************************************************/

#pragma once

#include "db/utils/atomic_intrinsics.h"

#include <cstdint>

#include "fmt/printf.h"

namespace graphquery::database::storage
{
    template<typename T, bool write = false>
    struct SRef_t
    {
        inline SRef_t() = default;

        inline SRef_t(T * _t, uint32_t * readers, uint8_t * writer): ref(_t), r_c(readers), w_c(writer) {}

        inline ~SRef_t()
        {
            if (r_c != nullptr && w_c != nullptr)
            {
                if constexpr (write)
                    utils::atomic_fetch_dec(*(&w_c));

                if constexpr (!write)
                    utils::atomic_fetch_dec(*(&r_c));
            }
        }

        SRef_t(const SRef_t & cpy)
        {
            ref = cpy.ref;
            r_c = cpy.r_c;
            w_c = cpy.w_c;
            cpy.~SRef_t();
            enter();
        }

        SRef_t & operator=(const SRef_t & cpy)
        {
            if (this != &cpy)
            {
                ref = cpy.ref;
                r_c = cpy.r_c;
                w_c = cpy.w_c;
                cpy.~SRef_t();
                enter();
            }
            return *this;
        }

        SRef_t(SRef_t && cpy) noexcept: ref(cpy.ref), r_c(cpy.r_c), w_c(cpy.w_c)
        {
            cpy.ref = nullptr;
            cpy.r_c = nullptr;
            cpy.w_c = nullptr;
        }

        SRef_t & operator=(SRef_t && cpy) noexcept
        {
            if (this != &cpy)
            {
                ref     = cpy.ref;
                r_c     = cpy.r_c;
                w_c     = cpy.w_c;
                cpy.ref = nullptr;
                cpy.r_c = nullptr;
                cpy.w_c = nullptr;
            }
            return *this;
        }

        inline void enter() const noexcept
        {
            if constexpr (write)
            {
                while (utils::atomic_load(&(*r_c)) != 0 || utils::atomic_load(&(*w_c)) != 0) {}
                utils::atomic_fetch_inc(&(*w_c));
            }

            if constexpr (!write)
            {
                while (utils::atomic_load(&(*w_c)) != 0) {}
                utils::atomic_fetch_inc(&(*r_c));
            }
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
        uint8_t * w_c  = nullptr;
    };
}; // namespace graphquery::database::storage