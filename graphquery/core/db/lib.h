#pragma once

#include <type_traits>

namespace graphquery::database
{
    template <typename T>
    T operator |(T lhs, T rhs) {
        using u_t = typename std::underlying_type<T>::type;
        return static_cast<T>(static_cast<u_t>(lhs) | static_cast<u_t>(rhs));
    }
}