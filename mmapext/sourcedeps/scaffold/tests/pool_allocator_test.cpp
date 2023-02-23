#include <assert.h>
#include <scaffold/pool_allocator.h>
#include <vector>

using namespace fo;

int main() {
    memory_globals::init();
    {

        const uint32_t nodes_per_pool = 4000;
        const uint32_t nodes_to_insert = 10 * nodes_per_pool;

        struct Node {
            uint32_t id;
            uint32_t score;
            uint32_t hp;
        };

        PoolAllocator pa(sizeof(Node), 4000);

        std::vector<Node *> nodes;

        for (uint32_t id = 0; id < nodes_to_insert; ++id) {
            Node *n = (Node *)pa.allocate(sizeof(Node), alignof(Node));
            n->id = id;
            n->score = id + 0xff;
            n->hp = 0xdead;
            nodes.push_back(n);
        }

        for (uint32_t id = 0; id < nodes_to_insert; id += 5) {
            assert(nodes[id]->id == id && nodes[id]->score == id + 0xff && nodes[id]->hp == 0xdead);
            pa.deallocate(nodes[id]);
            nodes[id] = nullptr;
        }

        for (uint32_t id = 0; id < nodes.size(); ++id) {
            if (nodes[id]) {
                assert(nodes[id]->id == id && nodes[id]->score == id + 0xff && nodes[id]->hp == 0xdead);
            }
            pa.deallocate(nodes[id]);
        }
    }

    memory_globals::shutdown();
}