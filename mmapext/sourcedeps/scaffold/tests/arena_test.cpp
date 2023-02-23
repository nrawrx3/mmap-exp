#include <scaffold/arena_allocator.h>
#include <scaffold/array.h>

#include <assert.h>
#include <random>

using RandomEngine = std::default_random_engine;
using IntegerDistrib = std::uniform_int_distribution<u32>;

using namespace fo;

template <size_t size> struct Block { u32 a[size]; };

void do_thing() {
    ArenaAllocator aa(fo::memory_globals::default_allocator(), 4096);
    fo::Array<Block<512>> blocks(aa);

    const u32 num_iters = 1000;

    fo::reserve(blocks, num_iters);

    for (u32 i = 0; i < num_iters; ++i) {
        fo::push_back(blocks, Block<512>());
        log_info("Pushed block - %u", i);
    }

    log_info("Total allocated after %lf KB iterations, or request for %u bytes = %lu",
             num_iters * 512 / 1024.0,
             num_iters * 512,
             aa.total_allocated());

    fo::Array<ArenaInfo> arena_chain;
    aa.get_chain_info(arena_chain);

    for (u32 i = 0; i < size(arena_chain); ++i) {
        auto &info = arena_chain[i];
        printf("%*cArena %u, size = %.3f, allocated = %.3f\n",
               i * 2,
               ' ',
               i,
               info.buffer_size / 1024.0,
               info.total_allocated / 1024.0);
    }
}

void realloc_test() {
    const u32 buffer_size = 4u << 20;
    ArenaAllocator aa(fo::memory_globals::default_allocator(), buffer_size);

    // First some random allocations

    fo::Array<u32> random_sizes{ 64, 32, 4, 16, 8, 64, 64, 128, 128, 512, 4, 8, 16, 32, 256 };

    fo::Array<void *> allocs;
    fo::reserve(allocs, fo::size(random_sizes));
    for (u32 s : random_sizes) {
        fo::push_back(allocs, aa.allocate(s, s > 16 ? 16 : s));
        assert(fo::back(allocs) != nullptr);
    }

    u32 last = fo::size(random_sizes) - 1;

    random_sizes[last] = random_sizes[last] * 2;
    allocs[last] = aa.reallocate(allocs[last], random_sizes[last]);
    assert(aa.allocated_size(allocs[last]) == random_sizes[last]);

    random_sizes[2] = 64;
    allocs[2] = aa.reallocate(allocs[2], random_sizes[2]);
    assert(aa.allocated_size(allocs[2]) == random_sizes[2]);

    random_sizes[last] = buffer_size;
    allocs[last] = aa.reallocate(allocs[last], random_sizes[last]);
    assert(aa.allocated_size(allocs[last]) == random_sizes[last]);
}

void realloc_test_with_sequence() {
    const u32 seed = 0xbadc0de;
    RandomEngine random_engine_0(seed);
    RandomEngine random_engine_1(seed);
    IntegerDistrib dist(0, 4096);

    const u32 nums_in_first_buffer = 1024;

    const u32 total_number_of_randoms = (128u << 20) / sizeof(u32); // 128 MB

    ArenaAllocator aa(memory_globals::default_allocator(), nums_in_first_buffer * sizeof(u32));

    Array<u32> default_array(memory_globals::default_allocator(), total_number_of_randoms);

    for (u32 i = 0; i < fo::size(default_array); ++i) {
        default_array[i] = dist(random_engine_0);
    }

    log_info("Done filling reference random sequence");

    Array<u32> tested_array(aa);
    fo::reserve(tested_array, nums_in_first_buffer);

    for (u32 i = 0; i < total_number_of_randoms; ++i) {
        fo::push_back(tested_array, dist(random_engine_1));
    }

    for (u32 i = 0; i < total_number_of_randoms; ++i) {
        assert(tested_array[i] == default_array[i]);
    }
}

int main() {

    memory_globals::init();

    {
        // do_thing();

        // realloc_test();
        realloc_test_with_sequence();
    }

    memory_globals::shutdown();
}