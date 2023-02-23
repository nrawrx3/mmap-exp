#pragma once

#include <scaffold/murmur_hash.h>
#include <scaffold/types.h>

#include <string>
#include <functional>
#include <unordered_map>

// Definitely disable on release build.
#if !defined(DEBUG) || defined(NDEBUG)
#    define ENABLE_SCAFFOLD_STRING_ID_TABLE 0
#endif

namespace fo {

struct SCAFFOLD_API StringId64 {
    u64 _string_id;

    inline StringId64(const char *str);

    operator u64() { return _string_id; }

    bool operator==(StringId64 o) { return _string_id == o._string_id; }
};

namespace internal {

struct StringIDTable {
    // TODO: Maybe use fo::Hash instead, and overload global operator new?
    std::unordered_map<u64, std::string> hashed_strings;
};

StringIDTable &global_string_id_table();

} // namespace internal

inline StringId64::StringId64(const char *str) {
    u64 hash = murmur_hash_64(str, sizeof(char) * strlen(str), SCAFFOLD_SEED);

#if defined(ENABLE_SCAFFOLD_STRING_ID_TABLE) || ENABLE_SCAFFOLD_STRING_ID_TABLE == 1
    auto &table = internal::global_string_id_table().hashed_strings;

    auto it = table.find(hash);

    if (it == table.end()) {
        table.insert(std::make_pair(hash, std::string(str)));
    } else if (strcmp(it->second.c_str(), str) != 0) {
        log_assert(
            false, "StringID - Hash collision between strings - '%s' and '%s'", str, it->second.c_str());
    } else {
        _string_id = hash;
    }

#else
    _string_id = hash;
#endif
}

} // namespace fo

namespace std {
template <> struct hash<fo::StringId64> {
    using argument_type = fo::StringId64;
    std::size_t operator()(const fo::StringId64 &id) const { return (std::size_t)(id._string_id); }
};

} // namespace std
