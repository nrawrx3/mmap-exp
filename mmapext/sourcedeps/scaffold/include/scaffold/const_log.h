#pragma once

#include <algorithm>
#include <stdint.h>
#include <type_traits>

#ifdef max
#    undef max
#endif

#ifdef min
#    undef min
#endif

namespace
{

/// Clips the given integer x to the closest power of 2 greater than or equal to x. Returns 0 if x is 0. x
/// must be < 2^(N - 1) where N is the number of bits of the unsigned integer representation.
template <typename T> inline constexpr T clip_to_pow2(T x)
{
    static_assert(std::is_integral<T>::value, "Must be integral");
    x = x - 1;
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    return x + 1;
}

template <typename T> inline constexpr bool is_power_of_2(T x)
{
    static_assert(std::is_integral<T>::value, "Must be integral");
    return (x & (x - 1)) == 0;
}

/// Returns floor(log_2(n))  (integer logarithm of n)
template <typename T> inline constexpr T log2_floor(T n)
{
    static_assert(std::is_integral<T>::value, "Must be integral");
    T i = 0;
    while (n > 1) {
        n = n / 2;
        ++i;
    }
    return i;
}

constexpr uint64_t _t[6] = { 0xFFFFFFFF00000000ull, 0x00000000FFFF0000ull, 0x000000000000FF00ull,
                             0x00000000000000F0ull, 0x000000000000000Cull, 0x0000000000000002ull };

inline constexpr uint64_t log2_ceil(uint64_t x)
{
    uint64_t y = (((x & (x - 1)) == 0) ? 0 : 1);
    uint64_t j = 32;

    for (uint32_t i = 0; i < 6; i++) {
        uint64_t k = (((x & _t[i]) == 0) ? 0 : j);
        y += k;
        x >>= k;
        j >>= 1;
    }

    return y;
}

/// Returns `ceil(a/b)`
template <typename T> static inline constexpr T ceil_div(T a, T b)
{
    static_assert(std::is_integral<T>::value, "integer types wanted");
    T mod = a % b;
    if (mod) {
        return a / b + 1;
    }
    return a / b;
}

template <typename T1, typename T2, typename T3> inline constexpr auto clamp(T3 value, T1 min, T2 max)
{
    return std::min(max, std::max(min, value));
}

template <typename V> bool equals_any(V) { return false; }

template <typename V, typename Head, typename... Rest> bool equals_any(V value, Head head, Rest... rest)
{
    return (value == head) || equals_any(value, rest...);
}

template <typename T> struct SizeofBits {
    static constexpr size_t value = sizeof(T) * 8;
};

} // namespace
