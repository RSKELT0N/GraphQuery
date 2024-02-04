/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file memory_ref.h
 * \brief Header describing a struct to interact with memory mapped
 *        files safely with reference counting.
 ************************************************************/

#pragma once

#include <cstdint>

namespace graphquery::database::storage
{
    template<typename T>
    struct SRef_t
    {
        inline SRef_t() = default;
        inline SRef_t(T * _t, uint32_t * _counter): ref(_t), counter(_counter) { ++(*counter); }

        inline ~SRef_t()
        {
            if (counter != nullptr)
                *counter -= *counter == 0 ? 0 : 1;
        }

        SRef_t(const SRef_t & cpy)
        {
            ref     = cpy.ref;
            counter = cpy.counter;
            ++(*counter);
        }

        SRef_t & operator=(const SRef_t & cpy)
        {
            ref     = cpy.ref;
            counter = cpy.counter;
            ++(*counter);
            return *this;
        }

        SRef_t(SRef_t && cpy) noexcept
        {
            ref         = cpy.ref;
            counter     = cpy.counter;
            cpy.ref     = nullptr;
            cpy.counter = nullptr;
        };

        SRef_t & operator=(SRef_t && cpy) noexcept
        {
            ref         = cpy.ref;
            counter     = cpy.counter;
            cpy.ref     = nullptr;
            cpy.counter = nullptr;
            return *this;
        }

        T * operator->() { return ref; }
        T * operator++(int) { return ref++; }
        T * operator--(int) { return ref--; }
        T * operator--() { return --ref; }
        T * operator++() { return ++ref; }
        T operator*() { return *ref; }

        T * ref            = nullptr;
        uint32_t * counter = nullptr;
    } __attribute__((packed));
}; // namespace graphquery::database::storage
