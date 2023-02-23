#include <scaffold/buddy_allocator.h>

#include <array>
#include <iostream>
#include <new>
#include <set>
#include <time.h>

constexpr uint32_t BUFFER_SIZE = 1 << 20;   // 1 MB
constexpr uint32_t SMALLEST_SIZE = 1 << 18; // 128 KB

template <uint32_t bytes> using Block = std::array<uint32_t, bytes / sizeof(uint32_t)>;

using SmallestBlock = Block<SMALLEST_SIZE>;

using Block_8KB = Block<8 << 10>;

int main() {
    fo::memory_globals::init();
    {
        using BA = fo::BuddyAllocator;
        BA ba(BUFFER_SIZE, SMALLEST_SIZE, true, fo::memory_globals::default_allocator());
        std::cout << "SIZE OF SMALLEST ARRAY = " << sizeof(SmallestBlock) << std::endl;

        auto b0 = ba.allocate(SMALLEST_SIZE, SMALLEST_SIZE);
        auto b1 = ba.allocate(SMALLEST_SIZE, SMALLEST_SIZE);
        auto b2 = ba.allocate(SMALLEST_SIZE, SMALLEST_SIZE);
        auto b3 = ba.allocate(SMALLEST_SIZE, SMALLEST_SIZE);

        ba.deallocate(b2);
        ba.deallocate(b1);
        ba.deallocate(b0);
        ba.deallocate(b3);

        // dbg here
        std::cout << ba.total_allocated() << "\n";
    }
    fo::memory_globals::shutdown();
}
