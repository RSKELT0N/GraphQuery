/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file atomic_intrinsics.h
 * \brief Header including platform dependent intrinsics for
 *        atomic operations.
 ************************************************************/

#pragma once

#include <cstdint>

namespace graphquery::database::utils
{

#if defined(__GNUC__) || defined(__clang__)
#elif defined(_MSC_VER)
#include <intrin.h>
#endif

    // ~ Support for integral atomic intrinsics

    template<typename T>
    inline T atomic_fetch_add(volatile T * variable, T value)
    {
#if defined(__GNUC__) || defined(__clang__)
        return __atomic_fetch_add(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
        return _InterlockedExchangeAdd(reinterpret_cast<volatile LONG *>(variable), value);
#endif
    }

    template<typename T>
    inline T atomic_fetch_sub(volatile T * variable, T value)
    {
#if defined(__GNUC__) || defined(__clang__)
        return __atomic_fetch_sub(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
        return _InterlockedExchangeAdd(reinterpret_cast<volatile LONG *>(variable), -value);
#endif
    }

    template<typename T>
    inline T atomic_fetch_xor(volatile T * variable, T value)
    {
#if defined(__GNUC__) || defined(__clang__)
        return __atomic_fetch_xor(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
        return _InterlockedXor(reinterpret_cast<volatile LONG *>(variable), value);
#endif
    }

    template<typename T>
    inline T atomic_fetch_and(volatile T * variable, T value)
    {
#if defined(__GNUC__) || defined(__clang__)
        return __atomic_fetch_and(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
        return _InterlockedAnd(reinterpret_cast<volatile LONG *>(variable), value);
#endif
    }

    template<typename T>
    inline T atomic_fetch_or(volatile T * variable, T value)
    {
#if defined(__GNUC__) || defined(__clang__)
        return __atomic_fetch_or(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
        return _InterlockedOr(reinterpret_cast<volatile LONG *>(variable), value);
#endif
    }

    template<typename T>
    inline T atomic_fetch_nand(volatile T * variable, T value)
    {
#if defined(__GNUC__) || defined(__clang__)
        return __atomic_fetch_nand(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
        T original = _InterlockedAnd(reinterpret_cast<volatile LONG *>(variable), ~value);
        return _InterlockedOr(reinterpret_cast<volatile LONG *>(variable), ~original);
#endif
    }

    template<typename T>
    inline bool atomic_fetch_cas(volatile T * variable, T & expected, T & new_value)
    {
#if defined(__GNUC__) || defined(__clang__)
        return __atomic_compare_exchange(variable, reinterpret_cast<T *>(&expected), reinterpret_cast<T *>(&new_value), false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
#elif defined(_MSC_VER)
        return _InterlockedCompareExchange(reinterpret_cast<volatile LONG *>(variable), new_value, expected);
#endif
    }

    template<typename T>
    inline T atomic_fetch_inc(volatile T * variable)
    {
        return atomic_fetch_add(variable, static_cast<T>(1));
    }

    template<typename T>
    inline T atomic_fetch_dec(volatile T * variable)
    {
        return atomic_fetch_sub(variable, static_cast<T>(1));
    }

    template<typename T>
    inline T atomic_load(volatile T * variable)
    {
#if defined(__GNUC__) || defined(__clang__)
        return __atomic_load_n(variable, __ATOMIC_RELAXED);
#elif defined(_MSC_VER)
        _ReadBarrier();
        return *variable;
#endif
    }

    template<typename T>
    inline void atomic_store(volatile T * variable, T value)
    {
#if defined(__GNUC__) || defined(__clang__)
        __atomic_store_n(variable, value, __ATOMIC_RELAXED);
#elif defined(_MSC_VER)
        *variable = value;
        _WriteBarrier();
#endif
    }

    template<typename T, typename I>
    inline void atomic_store(volatile T * variable, I value)
    {
#if defined(__GNUC__) || defined(__clang__)
        __atomic_store_n(variable, value, __ATOMIC_RELAXED);
#elif defined(_MSC_VER)
        *variable = value;
        _WriteBarrier();
#endif
    }

    // ~ Support for float/double atomic intrinsics

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"

    template<>
    inline bool atomic_fetch_cas(volatile double * variable, double & expected, double & new_value)
    {
#if defined(__GNUC__) || defined(__clang__)
        return __atomic_compare_exchange(reinterpret_cast<volatile uint64_t *>(variable),
                                         reinterpret_cast<uint64_t *>(&expected),
                                         reinterpret_cast<uint64_t *>(&new_value),
                                         false,
                                         __ATOMIC_ACQ_REL,
                                         __ATOMIC_RELAXED);
#elif defined(_MSC_VER)
        return _InterlockedCompareExchange64(reinterpret_cast<volatile LONG64 *>(variable), *reinterpret_cast<LONGLONG *>(&new_value), *reinterpret_cast<LONGLONG *>(&expected)) ==
               *reinterpret_cast<LONGLONG *>(&expected);
#endif
    }

    template<>
    inline bool atomic_fetch_cas(volatile float * variable, float & expected, float & new_value)
    {
#if defined(__GNUC__) || defined(__clang__)
        return __atomic_compare_exchange(reinterpret_cast<volatile uint32_t *>(variable),
                                         reinterpret_cast<uint32_t *>(&expected),
                                         reinterpret_cast<uint32_t *>(&new_value),
                                         false,
                                         __ATOMIC_ACQ_REL,
                                         __ATOMIC_RELAXED);
#elif defined(_MSC_VER)
        return _InterlockedCompareExchange(reinterpret_cast<volatile LONG *>(variable), *reinterpret_cast<LONG *>(&new_value), *reinterpret_cast<LONG *>(&expected)) ==
               *reinterpret_cast<LONG *>(&expected);
#endif
    }

    template<>
    inline double atomic_load(volatile double * variable)
    {
#if defined(__GNUC__) || defined(__clang__)
        uint64_t int_value = __atomic_load_n(reinterpret_cast<volatile uint64_t *>(variable), __ATOMIC_RELAXED);
        return *reinterpret_cast<double *>(&int_value);
#elif defined(_MSC_VER)
        _ReadBarrier();
        return *variable;
#endif
    }

    template<>
    inline float atomic_load(volatile float * variable)
    {
#if defined(__GNUC__) || defined(__clang__)
        uint32_t int_value = __atomic_load_n(reinterpret_cast<volatile uint32_t *>(variable), __ATOMIC_RELAXED);
        return *reinterpret_cast<float *>(&int_value);
#elif defined(_MSC_VER)
        _ReadBarrier();
        return *variable;
#endif
    }

#pragma GCC diagnostic pop

} // namespace graphquery::database::utils
