#include <scaffold/debug.h>
#include <scaffold/queue.h>

#include <algorithm>
#include <assert.h>

using namespace fo;

int main() {
    memory_globals::init();

    {
        Queue<int> q1(memory_globals::default_allocator());
        Queue<int> q2(memory_globals::default_allocator());

        auto pushback = [](Queue<int> &q) {
            for (int i = 0; i < 1024; ++i) {
                push_back(q, i);
            }
        };

        pushback(q1);

        for (int i = 0; i < 1024; ++i) {
            assert(*begin_front(q1) == i);
            pop_front(q1);
        }

        assert(size(q1) == 0);

        pushback(q2);

        assert(size(q2) == 1024);

        std::swap(q1, q2);

        assert(size(q2) == 0);
        assert(size(q1) == 1024);

        Queue<int> q3(memory_globals::default_allocator());

        reserve(q3, 10);

        push_back(q3, 0);
        push_back(q3, 1);
        push_back(q3, 2);
        push_back(q3, 3);
        push_back(q3, 4);
        push_back(q3, 5);

        // [0, 1, 2, 3, 4, 5]

        pop_front(q3);
        pop_front(q3);

        // [2, 3, 4, 5]

        push_back(q3, 6);
        push_back(q3, 7);
        push_back(q3, 8);

        // [2, 3, 4, 5, 6, 7, 8]

        auto ex = get_extent(q3, 0, 5);

        assert(ex.first_chunk_size + ex.second_chunk_size == 5);

        assert(ex.first_chunk_size == 5);

        assert(ex.first_chunk[0] == 2);
        assert(ex.first_chunk[1] == 3);
        assert(ex.first_chunk[2] == 4);
        assert(ex.first_chunk[3] == 5);
        assert(ex.first_chunk[4] == 6);

        // [-, -, 2, 3, 4, 5, 6, 7, 8, -]

        push_back(q3, 9);
        push_back(q3, 10);
        push_back(q3, 11);

        // [10, 11, 2, 3, 4, 5, 6, 7, 8, 9]
        // [2, 3, 4, 5, 6, 7, 8, 9, 10, 11]

        assert(size(q3) == 10);

        ex = get_extent(q3, 2, 10);

        assert(ex.first_chunk_size + ex.second_chunk_size == 10);
        assert(ex.first_chunk_size == 6);
    }

    memory_globals::shutdown();

    return 0;
}
