#include <scaffold/string_stream.h>

#include <stdarg.h>

namespace fo {
namespace string_stream {
Buffer &printf(Buffer &b, const char *format, ...) {
    va_list args;

    va_start(args, format);
    int n = vsnprintf(NULL, 0, format, args);
    va_end(args);

    uint32_t end = size(b);
    resize(b, end + n + 1);

    va_start(args, format);
    vsnprintf(begin(b) + end, n + 1, format, args);
    va_end(args);

    resize(b, end + n);

    return b;
}

Buffer &tab(Buffer &b, uint32_t column) {
    uint32_t current_column = 0;
    uint32_t i = size(b) - 1;
    while (i != 0xffffffffu && b[i] != '\n' && b[i] != '\r') {
        ++current_column;
        --i;
    }
    if (current_column < column)
        repeat(b, column - current_column, ' ');
    return b;
}

Buffer &repeat(Buffer &b, uint32_t count, char c) {
    for (uint32_t i = 0; i < count; ++i)
        push_back(b, c);
    return b;
}

CstrReturn c_str_own(Buffer &&b) {
    // Ensure there is a \0 at the end of the buffer.
    push_back(b, '\0');
    pop_back(b);

    CstrReturn ret;
    ret.c_str = b._data;
    ret.allocator = b._allocator;
    ret.length = size(b);

    b._data = nullptr;
    b._size = 0;

    return ret;
}

} // namespace string_stream
} // namespace fo
