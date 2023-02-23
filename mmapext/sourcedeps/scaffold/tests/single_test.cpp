#define CATCH_CONFIG_MAIN

#define SCAFFOLD_IMPLEMENTATION
#include "scaffold.h"

#include "catch.hpp"

#include <stdlib.h> // rand()

using fo::PodHash;
using fo::DyPackedUintArray;

TEST_CASE("DyPackedUintArray<> working correctly", "[DyPackedUintArray<>_works]") {
    fo::memory_globals::init();
    {
        DyPackedUintArray<> smallints{4, 512};
        smallints.set(0, 9);
        smallints.set(90, 12);
        smallints.set(91, 9);
        smallints.set(89, 10);
        smallints.set(80, 10);

        SECTION("get back value ok") {
            REQUIRE(smallints.get(0) == 9);
            REQUIRE(smallints.get(90) == 12);
            REQUIRE(smallints.get(91) == 9);
            REQUIRE(smallints.get(89) == 10);
            REQUIRE(smallints.get(80) == 10);
        }

        smallints.set(90, 0);
        smallints.set(91, 8);
        smallints.set(0, 7);

        SECTION("can reset") {
            REQUIRE(smallints.get(90) == 0);
            REQUIRE(smallints.get(91) == 8);
            REQUIRE(smallints.get(89) == 10);
            REQUIRE(smallints.get(80) == 10);
            REQUIRE(smallints.get(0) == 7);
        }

        {
            DyPackedUintArray<> bits{1, 1000};

            srand(0xbeef);

            auto is_set = fo::make_pod_hash<int, int>(fo::memory_globals::default_allocator());

            SECTION("Use as a bitset") {
                for (int i = 0; i < 1000; ++i) {
                    if (rand() % 1000 < 500) {
                        bits.set(i, 1);
                        set(is_set, i, 1);
                    } else {
                        set(is_set, i, 0);
                    }
                }

                for (auto i = cbegin(is_set), e = cend(is_set); i != e; ++i) {
                    if (i->value) {
                        REQUIRE(bits.get(i->key) == 1);
                    } else {
                        REQUIRE(bits.get(i->key) == 0);
                    }
                }
            }

            SECTION("resets ok") {
                bits.set(500, 0);
                bits.set(501, 0);
                REQUIRE(bits.get(501) == 0);
                REQUIRE(bits.get(500) == 0);
            }
        }

        SECTION("iterator") {
            const unsigned num_ints = 9990;
            DyPackedUintArray<> ints{4, num_ints};
            int loop_count = 0;
            for (auto i = ints.cbegin(), e = ints.cend(); i != e; ++i) {
                ++loop_count;
            }
            REQUIRE(loop_count == num_ints);
        }

        SECTION("smallintarray range set") {
            DyPackedUintArray<> ints{4, 9990};
            ints.set_range(100, 1000, 9);
            ints.set_range(1000, 2000, 8);
            for (int i = 100; i < 1000; ++i) {
                REQUIRE(ints.get(i) == 9);
            }
            for (int i = 1000; i < 2000; ++i) {
                REQUIRE(ints.get(i) == 8);
            }
            ints.set_range(101, 102, 7);
            for (int i = 101; i < 102; ++i) {
                REQUIRE(ints.get(i) == 7);
            }
            REQUIRE(ints.get(100) == 9);
            for (int i = 102; i < 1000; ++i) {
                REQUIRE(ints.get(i) == 9);
            }
#if 0
        ints.print();
#endif
        }
    }
    fo::memory_globals::shutdown();
}

TEST_CASE("DyPackedUintArray<> with TempAllocator",
          "[DyPackedUintArray<>_tempallocator]") {
    fo::memory_globals::init();
    {
        using Sia = DyPackedUintArray<>;

        const auto space = Sia::space_required(4, 512);
        REQUIRE(space == 256);
        fo::TempAllocator<space> temp_alloc;
        Sia smallints{4, 512, temp_alloc};

        smallints.set(0, 9);
        smallints.set(90, 12);
        smallints.set(91, 9);
        smallints.set(89, 10);
        smallints.set(80, 10);

        SECTION("get back value ok") {
            REQUIRE(smallints.get(0) == 9);
            REQUIRE(smallints.get(90) == 12);
            REQUIRE(smallints.get(91) == 9);
            REQUIRE(smallints.get(89) == 10);
            REQUIRE(smallints.get(80) == 10);
        }

        smallints.set(90, 0);
        smallints.set(91, 8);
        smallints.set(0, 7);

        SECTION("can reset") {
            REQUIRE(smallints.get(90) == 0);
            REQUIRE(smallints.get(91) == 8);
            REQUIRE(smallints.get(89) == 10);
            REQUIRE(smallints.get(80) == 10);
            REQUIRE(smallints.get(0) == 7);
        }
    }
    fo::memory_globals::shutdown();
}