#pragma once

#include <scaffold/murmur_hash.h>
#include <scaffold/pod_hash.h>
#include <assert.h>

namespace fo {
// Templates for usual hash and equal functions. Undefined.
template <typename T> uint64_t usual_hash(T const &k);
template <typename T> bool usual_equal(T const &k1, T const &k2);

// Define specializations here

/// cstring hash
template <> inline uint64_t usual_hash(char *const &s) {
    unsigned l = strlen(s);
    return murmur_hash_64(s, l, 0xDEADBEEF);
}

/// cstring equal
template <> inline bool usual_equal(char *const &s1, char *const &s2) { return strcmp(s1, s2) == 0; }

/// char hash
template <> inline uint64_t usual_hash(char const &s) { return s; }

/// char equal
template <> inline bool usual_equal(char const &s1, char const &s2) { return s1 == s2; }

/// int hash
template <> inline uint64_t usual_hash(int const &n) { return (uint64_t)n; }

template <> inline bool usual_equal(int const &n1, int const &n2) { return n1 == n2; }

/// int hash
template <> inline uint64_t usual_hash(uint64_t const &n) { return n; }

template <> inline bool usual_equal(uint64_t const &n1, uint64_t const &n2) { return n1 == n2; }

/// int hash
template <> inline uint64_t usual_hash(uint32_t const &n) { return (uint64_t)n; }

/// uint32_t hash
template <> inline bool usual_equal(uint32_t const &n1, uint32_t const &n2) { return n1 == n2; }

} // namespace fo
