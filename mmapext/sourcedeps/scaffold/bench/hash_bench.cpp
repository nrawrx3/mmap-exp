#include <benchmark/benchmark.h>
#include <scaffold/array.h>
#include <scaffold/const_log.h>
#include <scaffold/debug.h>
#include <scaffold/hash.h>
#include <scaffold/memory.h>
#include <scaffold/murmur_hash.h>
#include <scaffold/open_hash.h>
#include <scaffold/pod_hash.h>
#include <scaffold/pod_hash_usuals.h>

#include <array>
#include <assert.h>
#include <limits>
#include <stdlib.h>
#include <utility>

#include <unordered_map>

namespace fo {

template <> struct GetNilAndDeleted<u64> {
    static u64 get_nil() {
        return 8888;
    }

    static u64 get_deleted() {
        return 9999;
    }
};

}

using namespace fo;

static int address_variable = 0;

static void stdumap_hash_search(benchmark::State &bm_state) {

    std::function<uint64_t(const uint64_t &)> hash_u64 = [](const u64 &a) -> u64 { return a; };

    std::function<bool(const u64 &, const u64 &)> equal_u64 = [](const u64 &a, const u64 &b) -> bool {
        return a == b;
    };

    std::unordered_map<u64, u64, decltype(hash_u64), decltype(equal_u64)> h(1024, hash_u64, equal_u64);

    const uint32_t max_entries = bm_state.range(0);

    h.reserve(max_entries);

    for (u64 i = 0; i < max_entries; ++i) {
        h[i] = 0xdeadbeeflu;
    }

#if 0
    u64 key_to_find =
        ((u64(&stdumap_hash_search) >> 8) & u64(memory_globals::default_allocator)) % max_entries;
#endif
    u64 key_to_find = u64(&address_variable) % max_entries;

    ++address_variable;

    while (bm_state.KeepRunning()) {
        auto it = h.find(key_to_find);
        benchmark::DoNotOptimize(it);

        // key_to_find = (key_to_find + 1) % max_entries;
#if 0
        if (it == h.end()) {
            bm_state.SkipWithError("Should not happen");
        }
#endif
    }
}

static void open_hash_search(benchmark::State &bm_state) {
    memory_globals::init();
    {
        auto &alloc = memory_globals::default_allocator();

        using hash_type = OpenHash<u64, u64>;

        hash_type h{alloc, 16, [](const auto &i) { return i & 0xffffffffu; },
                    [](const auto &i, const auto &j) { return i == j; }};

        const uint64_t max_entries = bm_state.range(0);

        for (u64 i = 0; i < max_entries; ++i) {
            open_hash::set(h, i, (u64)0xdeadbeeflu);
        }

#if 0
        u64 key_to_find = ((u64(&open_hash_search) >> 8) & u64(memory_globals::default_allocator)) % max_entries;
#endif

        // u64 key_to_find = u64(&address_variable);
        u64 key_to_find = u64(&address_variable) % max_entries;

        while (bm_state.KeepRunning()) {
            auto idx = open_hash::find(h, key_to_find);
            benchmark::DoNotOptimize(idx);
            // key_to_find = (key_to_find + 1) % max_entries;
#if 0
                if (idx == open_hash::NOT_FOUND) {
                    bm_state.SkipWithError("Should not happen");
                }
#endif
        }
    }
    memory_globals::shutdown();
}

static void pod_hash_search(benchmark::State &bm_state) {
    memory_globals::init();
    {
        auto &alloc = memory_globals::default_allocator();

        using hash_type = PodHash<u64, u64, decltype(&usual_hash<u64>), decltype(&usual_equal<u64>)>;

        hash_type h(alloc, alloc, usual_hash<u64>, usual_equal<u64>);

        const uint64_t max_entries = bm_state.range(0);

        for (u64 i = 0; i < max_entries; ++i) {
            set(h, i, (u64)0xdeadbeeflu);
        }

#if 0
        u64 key_to_find = ((u64(&open_hash_search) >> 8) & u64(memory_globals::default_allocator)) % max_entries;
#endif

        // u64 key_to_find = ((u64)rand() + 1000) % max_entries;
        u64 key_to_find = u64(&address_variable) % max_entries;

        // auto end_iter = end(h);

        while (bm_state.KeepRunning()) {
            auto idx = get(h, key_to_find);
            benchmark::DoNotOptimize(idx);
            // key_to_find = (key_to_find + 1) % max_entries;
#if 0
                if (idx == end_iter) {
                    bm_state.SkipWithError("Should not happen");
                }
#endif
        }
    }
    memory_globals::shutdown();
}

constexpr uint32_t max_entries = 4096;

BENCHMARK(pod_hash_search)->RangeMultiplier(2)->Range(16, max_entries);
BENCHMARK(stdumap_hash_search)->RangeMultiplier(2)->Range(16, max_entries);
BENCHMARK(open_hash_search)->RangeMultiplier(2)->Range(16, max_entries);

BENCHMARK_MAIN();
