/************************************************************
* \author Ryan Skelton
 * \date 18/09/2023
 * \file atomic_intrinsics.h
 * \brief Header including platform dependent intrinsics for
 *        atomic operations.
 ************************************************************/

#pragma once

#if defined(__GNUC__) || defined(__clang__)
#include <cstdint>
#elif defined(_MSC_VER)
#include <intrin.h>
#endif

template <typename T>
T atomic_add(T* variable, T value) {
#if defined(__GNUC__) || defined(__clang__)
    return __atomic_fetch_add(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
    return _InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(variable), value);
#endif
}

template <typename T>
T atomic_sub(T* variable, T value) {
#if defined(__GNUC__) || defined(__clang__)
    return __atomic_fetch_sub(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
    return _InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(variable), -value);
#endif
}

template <typename T>
T atomic_xor(T* variable, T value) {
#if defined(__GNUC__) || defined(__clang__)
    return __atomic_fetch_xor(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
    return _InterlockedXor(reinterpret_cast<volatile LONG*>(variable), value);
#endif
}

template <typename T>
T atomic_and(T* variable, T value) {
#if defined(__GNUC__) || defined(__clang__)
    return __atomic_fetch_and(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
    return _InterlockedAnd(reinterpret_cast<volatile LONG*>(variable), value);
#endif
}

template <typename T>
T atomic_or(T* variable, T value) {
#if defined(__GNUC__) || defined(__clang__)
    return __atomic_fetch_or(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
    return _InterlockedOr(reinterpret_cast<volatile LONG*>(variable), value);
#endif
}

template <typename T>
T atomic_nand(T* variable, T value) {
#if defined(__GNUC__) || defined(__clang__)
    return __atomic_fetch_nand(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
    T original = _InterlockedAnd(reinterpret_cast<volatile LONG*>(variable), ~value);
    return _InterlockedOr(reinterpret_cast<volatile LONG*>(variable), ~original);
#endif
}

template <typename T>
bool atomic_cas(T* variable, T expected, T new_value) {
#if defined(__GNUC__) || defined(__clang__)
    return __atomic_compare_exchange_n(variable, expected, new_value);
#elif defined(_MSC_VER)
    return _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(variable), new_value, expected);
#endif
}

template <typename T>
T atomic_inc(T* variable) {
    return atomic_add(variable, static_cast<T>(1));
}

template <typename T>
T atomic_dec(T* variable) {
    return atomic_sub(variable, static_cast<T>(1));
}

template <typename T>
T atomic_exchange(T* variable, T new_value) {
#if defined(__GNUC__) || defined(__clang__)
    return __sync_lock_test_and_set(variable, new_value);
#elif defined(_MSC_VER)
    return _InterlockedExchange(reinterpret_cast<volatile LONG*>(variable), new_value);
#endif
}

template <typename T>
T atomic_load(const T* variable) {
#if defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(variable, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
    _ReadBarrier();
    return *variable;
#endif
}

template <typename T>
void atomic_store(T* variable, T value) {
#if defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(variable, value, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
    *variable = value;
    _WriteBarrier();
#endif
}
