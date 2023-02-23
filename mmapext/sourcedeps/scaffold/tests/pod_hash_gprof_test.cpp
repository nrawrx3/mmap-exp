#include <scaffold/memory.h>
#include <scaffold/debug.h>
#include <scaffold/pod_hash.h>
#include <scaffold/pod_hash_usuals.h>

int main() {
    using namespace fo;
    using namespace pod_hash;

    memory_globals::init();
    {
        PodHash<uint64_t, uint64_t> h{
            memory_globals::default_allocator(),
            memory_globals::default_allocator(),
            [](const uint64_t &key) {
                return murmur_hash_64(&key, sizeof(uint64_t), 0xCAFEBABE);
            },
            [](const uint64_t &key1, const uint64_t &key2) {
                return key1 == key2;
            }};

        const uint32_t MAX_ENTRIES = 1 << 20;

        pod_hash::reserve(h, MAX_ENTRIES);

        for (uint64_t i = 0; i < MAX_ENTRIES; ++i) {
			pod_hash::set(h, i, uint64_t(0xdeadbeeflu));
        }

        for (uint64_t i = 0; i < MAX_ENTRIES; ++i) {
            auto ret = pod_hash::get(h, i);
            assert(ret != end(h));
            assert(ret->value == 0xdeadbeeflu);
        }
        fprintf(stderr, "Done\n");
    }
    memory_globals::shutdown();
}
