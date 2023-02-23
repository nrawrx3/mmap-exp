/*
#pragma once

#include <stdint.h>

#ifndef alignof
#define alignof(x) __alignof(x)
#endif
*/

#define __STDC_FORMAT_MACROS

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

using i64 = int64_t;
using i32 = int32_t;
using i16 = int16_t;
using i8 = int8_t;

using f32 = float;
using f64 = double;

using uint = unsigned int;
using ulong = unsigned long;

using bool32 = i32;

using zu = size_t;

#if defined(_MSC_VER) && !defined(__PRETTY_FUNCTION__)
#    define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

// Check windows
#if _WIN32 || _WIN64
#    if _WIN64
#        define SCAFFOLD_64_BIT 1
#    else
#        define SCAFFOLD_32_BIT 1
#    endif
#endif

// Check GCC
#if defined(__GNUC__) || defined(__clang__)
#    if __x86_64__ || __ppc64__
#        define SCAFFOLD_64_BIT 1
#    else
#        define SCAFFOLD_32_BIT 1
#    endif
#endif

// Prefer using exact formats in printf/etc. But sometimes would like to just cast to lu or u to compile the
// thing first.
#define CAST_TO_LU(n) (long unsigned int)(n)
#define CAST_TO_U(n) (unsigned int)(n)

// Just a seed to use when you are out of ideas
#define SCAFFOLD_SEED 0xbadc0de
