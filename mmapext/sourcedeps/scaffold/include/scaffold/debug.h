#pragma once

#include <errno.h>
#include <stdio.h>
#include <stdlib.h> // abort()
#include <string.h>

#ifdef NDEBUG
#    define debug(M, ...)
#else
#    define debug(M, ...) fprintf(stderr, "DEBUG %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define clean_errno() (errno == 0 ? "errno is OK" : strerror(errno))

#define log_err(M, ...)                                                                                      \
    fprintf(stderr, "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)

#define log_warn(M, ...)                                                                                     \
    fprintf(stderr, "[WARN] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)

#define log_info(M, ...) fprintf(stderr, "[INFO] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define log_assert(cond, M, ...)                                                                             \
    if (!(cond)) {                                                                                           \
        fprintf(                                                                                             \
            stderr, "[ASSERT] (%s:%d: errno: %s)" M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__); \
        abort();                                                                                             \
    }

#define TOKENPASTE(x, y) x##y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)

#ifndef REALLY_INLINE
#    if defined(_MSC_VER)
#        define REALLY_INLINE __forceinline
#    else
#        define REALLY_INLINE __attribute__((always_inline)) inline
#    endif
#endif

#if defined(SCAFFOLD_API_EXPORT)
#    if defined(_MSC_VER)
#        define SCAFFOLD_API __declspec(dllexport)
#    else
#        define SCAFFOLD_API __attribute__((visibility("default")))
#    endif
#elif defined(SCAFFOLD_API_IMPORT)
#    if defined(_MSC_VER)
#        define SCAFFOLD_API __declspec(dllimport)
#    else
#        define SCAFFOLD_API __attribute__((visibility("default")))
#    endif
#else
#define SCAFFOLD_API
#endif

#if defined(_MSC_VER)
#    define SCAFFOLD_INTERNAL
#else
#    define SCAFFOLD_INTERNAL __attribute__((visibility("hidden")))
#endif

#if (__cplusplus >= 201703L || (defined _MSC_VER && _MSVC_LANG >= 201703L))
#    define SCAFFOLD_IF_CONSTEXPR constexpr
#else
#    define SCAFFOLD_IF_CONSTEXPR
#endif
