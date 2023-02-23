#pragma once

#include <scaffold/array.h>
#include <scaffold/collection_types.h>
#include <scaffold/memory.h>
#include <scaffold/murmur_hash.h>

#include <functional> // std::less and std::hash specialization
#include <stdio.h>
#include <string.h>

namespace fo {

/// Functions for operating on an Array<char> as a stream of characters, useful for string formatting, etc.
namespace string_stream {
typedef Array<char> Buffer;

/// Dumps the item to the stream using a default formatting.
Buffer &operator<<(Buffer &b, char c);
Buffer &operator<<(Buffer &b, const char *s);
Buffer &operator<<(Buffer &b, float f);
Buffer &operator<<(Buffer &b, int32_t i);
Buffer &operator<<(Buffer &b, uint32_t i);
Buffer &operator<<(Buffer &b, uint64_t i);
Buffer &operator<<(Buffer &b, double r);

// Mac related bs
#if defined __APPLE__
Buffer &operator<<(Buffer &b, size_t r);
#endif

/// Uses printf to print formatted data to the stream.
SCAFFOLD_API Buffer &printf(Buffer &b, const char *format, ...);

/// Pushes the raw data to the stream.
SCAFFOLD_API Buffer &push(Buffer &b, const char *data, uint32_t n);

/// Pads the stream with spaces until it is aligned at the specified column. Can be used to column align data.
/// (Assumes each char is 1 space wide, i.e. does not work with UTF-8 data.)
SCAFFOLD_API Buffer &tab(Buffer &b, uint32_t column);

/// Adds the specified number of c to the stream.
SCAFFOLD_API Buffer &repeat(Buffer &b, uint32_t count, char c);

/// Returns the stream as a C-string. There will always be a \0 character at the end of the returned string.
/// You don't have to explicitly add it to the buffer.
SCAFFOLD_API const char *c_str(Buffer &b);

/// Just because.
inline u32 length(Buffer &b) { return fo::size(b); }

/// Sometimes you may want to steal the allocated string into a plain-old `char *`. Call `c_str_own` to do
/// that. This of course means you are now in charge of freeing the memory...
struct CstrReturn {
    char *c_str;
    uint32_t length;
    fo::Allocator *allocator;
};

/// ... The moved-from buffer `b` can be reused as it it were a just now constructed Buffer.
SCAFFOLD_API CstrReturn c_str_own(Buffer &&b);

} // namespace string_stream

namespace string_stream_internal {
using namespace string_stream;

template <typename T> inline Buffer &printf_small(Buffer &b, const char *fmt, const T &t) {
    char s[32];
    snprintf(s, 32, fmt, t);
    return (b << s);
}
} // namespace string_stream_internal

namespace string_stream {
inline Buffer &operator<<(Buffer &b, char c) {
    push_back(b, c);
    return b;
}

inline Buffer &operator<<(Buffer &b, const char *s) { return push(b, s, (uint32_t)strlen(s)); }

inline Buffer &operator<<(Buffer &b, float f) { return string_stream_internal::printf_small(b, "%g", f); }

inline Buffer &operator<<(Buffer &b, int32_t i) { return string_stream_internal::printf_small(b, "%d", i); }

inline Buffer &operator<<(Buffer &b, uint32_t i) { return string_stream_internal::printf_small(b, "%u", i); }

inline Buffer &operator<<(Buffer &b, uint64_t i) {
    return string_stream_internal::printf_small(b, "%01llx", i);
}

inline Buffer &operator<<(Buffer &b, double r) { return string_stream_internal::printf_small(b, "%.5f", r); }

#if defined __APPLE__
inline Buffer &operator<<(Buffer &b, size_t r) { return string_stream_internal::printf_small(b, "%zu", r); }
#endif

inline Buffer &push(Buffer &b, const char *data, uint32_t n) {
    unsigned int end = size(b);
    resize(b, end + n);
    memcpy(begin(b) + end, data, n);
    return b;
}

inline const char *c_str(Buffer &b) {
    // Ensure there is a \0 at the end of the buffer. This works because `Array::pop_back` never actually
    // frees its memory until it is deleted or moved-from.
    push_back(b, '\0');
    pop_back(b);
    return begin(b);
}

} // namespace string_stream
} // namespace fo

// Specialization of std::less and std::hash
namespace std {

template <> struct less<fo::string_stream::Buffer> {
    bool operator()(const fo::string_stream::Buffer &str1, const fo::string_stream::Buffer &str2) const {
        const u32 count = std::min(fo::size(str1), fo::size(str2));
        return strncmp(fo::data(str1), fo::data(str2), count) < 0;
    }
};

template <> struct hash<fo::string_stream::Buffer> {
    std::size_t operator()(const fo::string_stream::Buffer &str1) const {
        return (std::size_t)fo::murmur_hash_64(fo::data(str1), fo::size(str1), SCAFFOLD_SEED);
    }
};

} // namespace std
