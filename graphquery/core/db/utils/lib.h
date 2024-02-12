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
#include <algorithm>

namespace graphquery::database::utils
{
    template<typename T>
    struct STimedResult_t
    {
        T result;
        std::chrono::duration<double> elapsed;
    };

    template<>
    struct STimedResult_t<void>
    {
        std::chrono::duration<double> elapsed;
    };

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

    inline int64_t ceilaferdiv(const uint64_t _x, const uint64_t _y)
    {
        return 1 + ((_x - 1) / _y);
    }

    // Converts a string to lowercase
    inline std::string to_lower_case(std::string to_convert)
    {
        for (char & i : to_convert)
            i = i | 32;

        return to_convert;
    }

    // Converts a string to lowercase
    inline std::string to_upper_case(std::string to_convert)
    {
        for (char & i : to_convert)
            i = i & ~32;

        return to_convert;
    }

    inline std::vector<std::string_view> split(std::string_view in, const char sep)
    {
        std::vector<std::string_view> ret;
        ret.reserve(std::count(in.begin(), in.end(), sep) + 1); // optional
        for (auto p = in.begin();; ++p)
        {
            auto q = p;
            p      = std::find(p, in.end(), sep);
            ret.emplace_back(q, p);
            if (p == in.end())
                return ret;
        }
    }

    template<typename Ret, typename Func, typename Obj, typename... Args>
    inline constexpr auto measure(Func && func, const Obj & obj, Args &&... args) -> STimedResult_t<Ret>
    {
        const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        auto func_result                                  = (obj->*func)(args...);
        const std::chrono::steady_clock::time_point end   = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed       = end - start;

        return STimedResult_t<Ret> {func_result, elapsed};
    }

    template<typename Func, typename Obj, typename... Args>
    inline constexpr auto measure(Func && func, const Obj & obj, Args &&... args) -> STimedResult_t<void>
    {
        const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        (obj->*func)(args...);
        const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed     = end - start;

        return STimedResult_t<void> {elapsed};
    }
} // namespace graphquery::database::utils
