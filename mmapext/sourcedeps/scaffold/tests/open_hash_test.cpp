#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <iostream>
#include <scaffold/open_hash.h>

struct GetNilAndDeleted__uint64_t {
    static constexpr uint64_t get_nil() { return 8888; }
    static constexpr uint64_t get_deleted() { return 8888; }
};

TEST_CASE("OpenHash find", "[OpenHash_find]") {
    fo::memory_globals::init();

    auto &alloc = fo::memory_globals::default_allocator();

    using hash_type = fo::OpenHash<uint64_t, uint64_t, GetNilAndDeleted__uint64_t>;

    namespace open_hash = fo::open_hash;

    {
        hash_type h{ alloc,
                     16,
                     [](const auto &i) { return i & 0xffffffffu; },
                     [](const auto &i, const auto &j) { return i == j; } };

        for (uint64_t k = 0; k < 1024; ++k) {
            for (uint64_t prev_k = 0; prev_k < k; ++prev_k) {
                auto i = open_hash::find(h, prev_k);
                REQUIRE(i != open_hash::NOT_FOUND);
                REQUIRE(open_hash::value(h, i) == 1024 - prev_k);
            }
            open_hash::set(h, k, 1024 - k);
        }
    } // namespace open_hash=fo::open_hash;

    fo::memory_globals::shutdown();
}

TEST_CASE("OpenHash remove", "[OpenHash_remove_rehash]") {
    fo::memory_globals::init();

    auto &alloc = fo::memory_globals::default_allocator();

    using hash_type = fo::OpenHash<uint64_t, uint64_t, GetNilAndDeleted__uint64_t>;

    namespace open_hash = fo::open_hash;

    {
        hash_type h{ alloc,
                     16,
                     [](const auto &i) { return i & 0xffffffffu; },
                     [](const auto &i, const auto &j) { return i == j; } };

        for (uint64_t k = 0; k < 1024; ++k) {
            open_hash::set(h, k, 1024 - k);
        }

        // Remove half
        for (uint64_t k = 0; k < 1024 / 2; ++k) {
            open_hash::remove(h, k);
        }
        // Insert that half again - should not reallocate array with larger
        // size
        uint32_t size = h._num_slots;
        for (uint64_t k = 0; k < 1024 / 2; ++k) {
            open_hash::set(h, k, 1024 - k);
        }

        for (uint64_t k = 0; k < 1024; ++k) {
            uint32_t i = open_hash::find(h, k);
            REQUIRE(i != open_hash::NOT_FOUND);
            REQUIRE(open_hash::value(h, i) == 1024 - k);
        }
    } // namespace open_hash=fo::open_hash;

    fo::memory_globals::shutdown();
}

TEST_CASE("OpenHash key insert only", "[OpenHash_insert_key]") {
    fo::memory_globals::init();

    auto &alloc = fo::memory_globals::default_allocator();

    using hash_type = fo::OpenHash<uint64_t, uint64_t, GetNilAndDeleted__uint64_t>;

    namespace open_hash = fo::open_hash;
    {
        hash_type h{ alloc,
                     16,
                     [](const auto &i) { return i & 0xffffffffu; },
                     [](const auto &i, const auto &j) { return i == j; } };

        for (uint64_t k = 0; k < 1024; ++k) {
            auto index = open_hash::insert_key(h, k);
            open_hash::value(h, index) = 1024 - k;
        }

        // Remove half
        for (uint64_t k = 0; k < 1024 / 2; ++k) {
            open_hash::remove(h, k);
        }
        // Insert that half again - should not reallocate array with larger
        // size
        uint32_t size = h._num_slots;
        for (uint64_t k = 0; k < 1024 / 2; ++k) {
            open_hash::set(h, k, 1024 - k);
        }

        for (uint64_t k = 0; k < 1024; ++k) {
            uint32_t i = open_hash::find(h, k);
            REQUIRE(i != open_hash::NOT_FOUND);
            REQUIRE(open_hash::value(h, i) == 1024 - k);
        }
    } // namespace open_hash=fo::open_hash;

    fo::memory_globals::shutdown();
}

TEST_CASE("Copy,Move,Assign", "[OpenHash_copy_move_assign]") {
    fo::memory_globals::init();
    {
        auto &alloc = fo::memory_globals::default_allocator();

        using hash_type = fo::OpenHash<uint64_t, uint64_t, GetNilAndDeleted__uint64_t>;

        namespace open_hash = fo::open_hash;

        {
            hash_type h{ alloc,
                         16,
                         [](const auto &i) { return i & 0xffffffffu; },
                         [](const auto &i, const auto &j) { return i == j; } };

            for (uint64_t k = 0; k < 1024; ++k) {
                auto index = open_hash::insert_key(h, k);
                open_hash::value(h, index) = 1024 - k;
            }

            // Remove half
            for (uint64_t k = 0; k < 1024 / 2; ++k) {
                open_hash::remove(h, k);
            }

            hash_type h1 = h;
            for (uint64_t k = 1024 / 2; k < 1024; ++k) {
                REQUIRE(open_hash::must_value(h, k) == open_hash::must_value(h1, k));
            }

            h1 = std::move(h);
            for (uint64_t k = 1024 / 2; k < 1024; ++k) {
                REQUIRE(open_hash::must_value(h1, k) == 1024 - k);
            }
        } // namespace open_hash=fo::open_hash;
        fo::memory_globals::shutdown();
    }
}

TEST_CASE("value_default", "[value_default]") {
    fo::memory_globals::init();
    {
        auto &alloc = fo::memory_globals::default_allocator();

        using hash_type = fo::OpenHash<uint64_t, uint64_t, GetNilAndDeleted__uint64_t>;

        namespace open_hash = fo::open_hash;

        {
            hash_type h{ alloc,
                         16,
                         [](const auto &i) { return i & 0xffffffffu; },
                         [](const auto &i, const auto &j) { return i == j; } };

            constexpr u32 count = 10;

            for (uint64_t k = 0; k < count; ++k) {
                auto index = open_hash::insert_key(h, k);
                open_hash::value(h, index) = count - k;
            }

            // Remove half
            for (uint64_t k = 0; k < count / 2; ++k) {
                open_hash::remove(h, k);
            }

            const auto print_hash = [&]() {
                for (uint32_t i = 0; i < h._num_slots; ++i) {
                    auto keys = (uint64_t *)(h._buffer + h._keys_offset);
                    auto values = (uint64_t *)(h._buffer + h._values_offset);

                    auto v = (keys[i] != 8888 && keys[i] != 9999) ? values[keys[i]] : 999999;

                    std::cout << "Key: " << i << " = " << keys[i] << " Value = " << v << " \n";
                }

                std::cout << "--\n";
            };

            // Reinsert
            // std::cout << "Reinserting...\n";
            for (uint64_t k = 0; k < count / 2; ++k) {
                std::cout << "Inserting: " << k << "\n";
                uint64_t &v = open_hash::value_default(h, k);
                v = count - k;
            }

            for (uint64_t k = 0; k < count; ++k) {
                // std::cout << "k = " << k << " => " << open_hash::must_value(h, k) << "\n";
                REQUIRE(open_hash::must_value(h, k) == count - k);
            }
        } // namespace open_hash=fo::open_hash;
        fo::memory_globals::shutdown();
    }
}

TEST_CASE("OpenHash iterator", "[OpenHash_iterator]") {
    fo::memory_globals::init();
    {
        auto &alloc = fo::memory_globals::default_allocator();

        using hash_type = fo::OpenHash<uint64_t, uint64_t, GetNilAndDeleted__uint64_t>;

        namespace open_hash = fo::open_hash;

        {
            hash_type h{ alloc,
                         16,
                         [](const auto &i) { return i & 0xffffffffu; },
                         [](const auto &i, const auto &j) { return i == j; } };

            constexpr u32 count = 10;

            for (uint64_t k = 0; k < count; ++k) {
                auto index = open_hash::insert_key(h, k);
                open_hash::value(h, index) = count - k;
            }

            // Remove half
            for (uint64_t k = 0; k < count / 2; ++k) {
                open_hash::remove(h, k);
            }

            const auto print_hash = [&]() {
                for (uint32_t i = 0; i < h._num_slots; ++i) {
                    auto keys = (uint64_t *)(h._buffer + h._keys_offset);
                    auto values = (uint64_t *)(h._buffer + h._values_offset);

                    auto v = (keys[i] != 8888 && keys[i] != 9999) ? values[keys[i]] : 999999;

                    std::cout << "Key: " << i << " = " << keys[i] << " Value = " << v << " \n";
                }
                std::cout << "--\n";
            };

            i32 i = 0;
            for (auto it = begin(h); it != end(h); ++it) {
                printf("i = %i, key = %lu\n", i, (*it).key);
                REQUIRE((*it).value == count - (*it).key);
            }

            // Reinsert
            // std::cout << "Reinserting...\n";
            for (uint64_t k = 0; k < count / 2; ++k) {
                // print_hash();
                uint64_t &v = open_hash::value_default(h, k);
                v = count - k;
            }
            // print_hash();

            for (uint64_t k = 0; k < count; ++k) {
                // std::cout << "k = " << k << " => " << open_hash::must_value(h, k) << "\n";
                REQUIRE(open_hash::must_value(h, k) == count - k);
            }
        } // namespace open_hash=fo::open_hash;
    }
    fo::memory_globals::shutdown();
}
