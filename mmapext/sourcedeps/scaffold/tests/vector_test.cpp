#include <scaffold/vector.h>
#include <scaffold/temp_allocator.h>
#include <time.h>

struct PodType {
    int a;
};

int recurse_depth_max = 400;

int recurse(int depth) {
    if (depth < recurse_depth_max) {
        return 0;
    }
    int x = (time(nullptr) % depth);
    return x + recurse(depth + 1);
}

int main() {

    fo::memory_globals::init();
    {
        // Pollute the stack with a recurse function call like this ??
        recurse(10);

        fo::TempAllocator1024 ta;

        fo::Vector<PodType> vec(ta);
        fo::resize(vec, 400);

        for (auto &e : vec) {
            printf("%i| ", e.a);
        }
    }

    {
    	fo::Vector<int> v;
    	fo::push_back(v, 1);
    }

    fo::memory_globals::shutdown();
}