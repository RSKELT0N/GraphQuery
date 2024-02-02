/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file lib.h
 * \brief Header of commonly used functions within the core
 *        segment of the program, which can be included and
 *        utilised by other translation units.
 ************************************************************/

#pragma once

#include <climits>
#include <type_traits>

namespace graphquery::database
{
    template<typename T>
    T operator|(T lhs, T rhs)
    {
        using u_t = typename std::underlying_type_t<T>;
        return static_cast<T>(static_cast<u_t>(lhs) | static_cast<u_t>(rhs));
    }

    inline int32_t abs(const int32_t val) noexcept
    {
        int const mask = val >> (sizeof(int) * CHAR_BIT - 1);
        return (val + mask) ^ mask;
    }

    inline int64_t ceil(const uint64_t _x, const uint64_t _y)
    {
        return 1 + ((_x - 1) / _y);
    }
} // namespace graphquery::database
