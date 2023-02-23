#pragma once

#include <scaffold/debug.h>
#include <scaffold/types.h>

namespace fo {

/// Implementation of the 64 bit MurmurHash2 function http://murmurhash.googlepages.com/
SCAFFOLD_API uint64_t murmur_hash_64(const void *key, uint32_t len, uint64_t seed);
} // namespace fo
