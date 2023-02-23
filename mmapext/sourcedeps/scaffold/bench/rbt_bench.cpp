#include <benchmark/benchmark.h>
#include <scaffold/rbt.h>
#include <vector>

using namespace fo;

static void insert_in_sorted_order(benchmark::State &bm_state) {
    memory_globals::init();
    {
        auto &alloc = memory_globals::default_allocator();

        rbt::RBTree<u64, u64> tree(alloc);

        const u64 max_entries = bm_state.range(0);

        while (bm_state.KeepRunning()) {
            for (u64 i = 0; i < max_entries; ++i) {
                auto res = rbt::set(tree, i, i);
                benchmark::DoNotOptimize(res);
            }
        }
    }
    memory_globals::shutdown();
}

static void find_in_rbt(benchmark::State &bm_state) {
    memory_globals::init();
    {
        auto &alloc = memory_globals::default_allocator();

        rbt::RBTree<u64, u64> tree(alloc);

        const u64 max_entries = bm_state.range(0);

        for (u64 i = 0; i < max_entries; ++i) {
            rbt::set(tree, i, i);
        }

#if 0
        std::vector<u64> keys_to_find;
        keys_to_find.reserve(max_entries);

        for (u64 i = 0; i < max_entries; ++i) {
            keys_to_find.push_back(u64(rand() % (u64)max_entries));
        }

#endif
        // u64 key_to_find = ((u64(&find_in_rbt) >> 8) & u64(memory_globals::default_allocator)) % max_entries;
        u64 key_to_find = ((u64)rand() + 1000) % max_entries;

        while (bm_state.KeepRunning()) {
            auto res = rbt::get(tree, key_to_find);
            benchmark::DoNotOptimize(res);

            key_to_find = (key_to_find + 1) % max_entries;
        }
    }
    memory_globals::shutdown();
}

constexpr uint32_t max_entries = 4096;

// BENCHMARK(insert_in_sorted_order)->RangeMultiplier(2)->Range(2, max_entries);
BENCHMARK(find_in_rbt)->RangeMultiplier(2)->Range(2, max_entries);

BENCHMARK_MAIN();
