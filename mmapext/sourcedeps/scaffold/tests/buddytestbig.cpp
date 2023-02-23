#include <scaffold/buddy_allocator.h>
#include <scaffold/one_time_allocator.h>
#include <scaffold/timed_block.h>

#include <array>
#include <iostream>
#include <new>
#include <random>
#include <set>
#include <stdio.h>
#include <time.h>

using BA = fo::BuddyAllocator;

constexpr uint64_t BUFFER_SIZE = u64(4) << 30; // 4 GB
constexpr uint64_t SMALLEST_SIZE = 4 << 10;    // 4 KB
constexpr uint64_t NUM_INDICES = BUFFER_SIZE / SMALLEST_SIZE;

template <uint64_t bytes> using Block = std::array<uint64_t, bytes / sizeof(uint64_t)>;

using SmallestBlock = Block<SMALLEST_SIZE>;

template <typename BlockTy> constexpr uint64_t _align_of() { return alignof(BlockTy); }

using Block_8KB = Block<8 << 10>;

uint64_t g_seed;

void alloc_one_full_buddy() {
    BA ba(BUFFER_SIZE, SMALLEST_SIZE, true, fo::memory_globals::default_allocator());

    uint32_t *buddy_mem = nullptr;
    {
        TIMED_BLOCK;
        buddy_mem = reinterpret_cast<uint32_t *>(ba.allocate(BUFFER_SIZE, 16));
    }

    log_assert(buddy_mem != nullptr, "BuddyAllocator failed to allocate block");

    {
        // Write something into memory to avoid getting optimized out
        for (uint32_t i = 0; i < BUFFER_SIZE / sizeof(uint32_t); ++i) {
            buddy_mem[i] = i;
        }
    }

    ba.deallocate(buddy_mem);

    // Print results
    printf("Results for %s\n", __PRETTY_FUNCTION__);
    timedblock::print_record_table(stdout);
    timedblock::reset();
}

struct AllocResult {
    BA *ba;
    std::vector<void *> blocks;
};

AllocResult alloc_all_leaves() {
    auto ba = new BA(BUFFER_SIZE, SMALLEST_SIZE, true, fo::memory_globals::default_allocator());

    puts("Press enter to continue...");
    getchar();

    std::default_random_engine dre(g_seed);
    std::uniform_int_distribution<int> d(1, 10);

    std::vector<void *> blocks;
    blocks.reserve(BUFFER_SIZE / SMALLEST_SIZE + 1);

    while (true) {
        auto block = ba->allocate(sizeof(SmallestBlock), alignof(SmallestBlock));
        if (!block) {
            break;
        }
        blocks.push_back(block);
    }

    return AllocResult{ba, std::move(blocks)};
}

void deallocate_swapped(AllocResult res) {
    log_assert(res.blocks.size() > 1, "");

    for (size_t i = 0; i < res.blocks.size() - 1; i += 2) {
        std::swap(res.blocks[i], res.blocks[i + 1]);
    }

    for (void *p : res.blocks) {
        res.ba->deallocate(p);
    }

    delete res.ba;

    printf("Results for %s\n", __PRETTY_FUNCTION__);
    printf("Blocks allocated: %zu\n", res.blocks.size());
    timedblock::print_record_table(stdout);
    fflush(stdout);
}

void deallocate_random(AllocResult res) {
    log_assert(res.blocks.size() > 1, "");

    std::default_random_engine dre(g_seed);
    std::uniform_int_distribution<size_t> d(0, res.blocks.size() - 1);

    const size_t times_to_swap = std::max(size_t(10000), res.blocks.size());

    for (size_t i = 0; i < times_to_swap; ++i) {
        std::swap(res.blocks[d(dre)], res.blocks[d(dre)]);
    }

    for (void *p : res.blocks) {
        res.ba->deallocate(p);
    }

    delete res.ba;

    printf("Results for %s\n", __PRETTY_FUNCTION__);
    printf("Blocks allocated: %zu\n", res.blocks.size());
    timedblock::print_record_table(stdout);
    fflush(stdout);
}

AllocResult alloc_variable_chunks(fo::OneTimeAllocator *one_time_alloc) {
    auto ba = new BA(BUFFER_SIZE, SMALLEST_SIZE, true, *one_time_alloc);

    std::vector<void *> blocks;
    blocks.reserve(BUFFER_SIZE / SMALLEST_SIZE + 1);

    std::default_random_engine dre(g_seed);
    std::uniform_int_distribution<size_t> d(0, 100);

    while (true) {
        const size_t random = d(dre);

        size_t chunk_size = 0;

        if (random < 70) {
            chunk_size = SMALLEST_SIZE;
        } else if (random < 90) {
            chunk_size = SMALLEST_SIZE * 2;
        } else if (random < 95) {
            chunk_size = SMALLEST_SIZE * 4;
        } else {
            chunk_size = SMALLEST_SIZE * 8;
        }

        auto block = ba->allocate(chunk_size, 16);

        if (!block) {
            break;
        }

        blocks.push_back(block);
    }

    log_info("Allocated: %zu blocks", blocks.size());

    return AllocResult{ba, std::move(blocks)};
}

int main(int argc, char **argv) {
    g_seed = argc >= 2 ? strtoull(argv[1], nullptr, 10) : 100;

    log_info("Seed used = %lu\n", g_seed);

    fo::memory_globals::init();
    {
        //
        alloc_one_full_buddy();

        timedblock::reset();

#if 1
        //
        {
            fo::OneTimeAllocator one_time_alloc;
            deallocate_swapped(alloc_variable_chunks(&one_time_alloc));
        }
#endif

        timedblock::reset();

#if 1
        //
        {
            fo::OneTimeAllocator one_time_alloc;
            deallocate_random(alloc_variable_chunks(&one_time_alloc));
        }

#endif
    }
    fo::memory_globals::shutdown();
    fprintf(stderr, "Seed = %lu\n", g_seed);
}
