#include <scaffold/debug.h>
#include <scaffold/math_types.h>
#include <scaffold/vector.h>

#include <algorithm>
#include <assert.h>
#include <vector>

using namespace fo;

template <typename T> using NVector = Vector<T>;

int main() {
    memory_globals::init();
    {
        {
            NVector<int> arr1;
            NVector<int> arr2;

            for (int i = 0; i < 1024; ++i) {
                push_back(arr1, 1);
            }

            for (int i = 0; i < 5120; ++i) {
                push_back(arr2, 2);
            }

            std::swap(arr1, arr2);

            assert(size(arr2) == 1024);
            assert(size(arr1) == 5120);

            assert(std::all_of(arr1.begin(), arr1.end(), [](int i) { return i == 2; }));

            assert(std::all_of(arr2.begin(), arr2.end(), [](int i) { return i == 1; }));

            // Move
            arr1 = std::move(arr2);
            assert(size(arr1) == 1024);
            assert(size(arr2) == 0);
            assert(std::all_of(arr1.begin(), arr1.end(), [](int i) { return i == 1; }));

            // Reuse arr2
            resize(arr2, 2000);
            log_assert(size(arr2) == 2000, "");
            log_info("DONE RESIZING");
            for (int i = 0; i < 2000; ++i) {
                arr2[i] = 0xbeef;
            }
            log_info("DONE FILLING");
            assert(std::all_of(arr2.begin(), arr2.end(), [](int i) { return i == 0xbeef; }));

            log_info("DONE CHECKING");
        }
        {
            NVector<std::vector<int>> arr1;
            NVector<std::vector<int>> arr2;

            using init_list = std::initializer_list<int>;

#if 0
            emplace_back(arr1, {1, 2, 3});
            emplace_back(arr2, {4, 5, 6});
#endif

            for (int i = 0; i < 100; i += 3) {
                auto &v1 = emplace_back(arr1, init_list{i, i + 1, i + 2});
                log_info("%i - size = %zu, [%i, %i, %i]", i, v1.size(), v1[0], v1[1], v1[2]);
                auto &v2 = emplace_back(arr2, init_list{200 + i, 200 + i + 1, 200 + i + 2});
                log_info("%i - size = %zu, [%i, %i, %i]", i, v2.size(), v2[0], v2[1], v2[2]);
            }

            for (int i = 0; i < 100; i += 3) {
                const auto &v1 = arr1[i / 3];
                log_info("%i - size = %zu, [%i, %i, %i]", i, v1.size(), v1[0], v1[1], v1[2]);

                assert(v1.size() == 3 && v1[0] == i && v1[1] == (i + 1) && v1[2] == (i + 2));

                const auto &v2 = arr2[i / 3];
                assert(v2.size() == 3 && v2[0] == (200 + i) && v2[1] == (200 + i + 1) &&
                       v2[2] == (200 + i + 2));
            }
        }
    }
    memory_globals::shutdown();
}
