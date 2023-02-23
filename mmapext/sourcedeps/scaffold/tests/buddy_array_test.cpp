#include <scaffold/array.h>
#include <scaffold/buddy_allocator.h>

#include <stdio.h>

constexpr uint32_t BUFFER_SIZE = 64 << 10; // 64 KB
constexpr uint32_t SMALLEST_SIZE = 8;      // 8 bytes

template <size_t size> struct alignas(16) Ob { char _arr[size]; };

int main() {
    using namespace fo;

    memory_globals::init();
    {
        const size_t object_sz = 16;

        BuddyAllocator ba(BUFFER_SIZE, object_sz, true, memory_globals::default_scratch_allocator());

        Array<Ob<object_sz>> a{ba};
        for (uint32_t size = 16; size <= BUFFER_SIZE / 4; size *= 2) {
            log_info("New size = %u bytes", size);
            resize(a, size / object_sz);
        }
        clear(a);
    }

    memory_globals::shutdown();
}
