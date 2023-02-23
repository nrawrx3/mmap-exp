#include <scaffold/pool_allocator.h>

#include <scaffold/debug.h>

#include <cassert>
#include <new>
#include <stdlib.h>
#include <string.h>

static constexpr uint64_t END_NUMBER = 0xffffffffu;

// Node number to corresponding node's memory
static inline uint32_t *num_to_mem(fo::PoolAllocator *a, uint32_t node_num) {
    char *nodes = (char *)fo::memory::align_forward(a->_mem + sizeof(fo::PoolAllocator), 16u);
    return (uint32_t *)(nodes + node_num * a->_node_size);
}

namespace fo {

// Initializes an allocator that doesn't own any buffer
PoolAllocator::PoolAllocator()
    : _node_size(0)
    , _num_nodes(0)
    , _nodes_allocated(0)
    , _first_free(0)
    , _mem(nullptr)
    , _backing(nullptr) // Backing should be set when allocator begins owning a buffer
{}

PoolAllocator::PoolAllocator(uint64_t node_size, uint64_t num_nodes, Allocator &backing)
    : _node_size(node_size)
    , _num_nodes(num_nodes)
    , _nodes_allocated(0)
    , _first_free(0)
    , _mem(nullptr)
    , _backing(&backing) {

    assert(node_size >= sizeof(uint64_t));

    // _mem = (char *)calloc(1, sizeof(PoolAllocator) + 16 + node_size * num_nodes);
    _mem = (char *)_backing->allocate(sizeof(PoolAllocator) + 16 + node_size * num_nodes, 16u);

    log_assert(uintptr_t(_mem) % 16 == 0, "");

    // Default construct next allocator to denote it doesn't own any memory
    new (_mem) PoolAllocator;

    char *_nodes = (char *)fo::memory::align_forward(_mem + sizeof(PoolAllocator), 16u);

    uint32_t i = 1;
    for (char *h = _nodes; h < _nodes + _node_size * _num_nodes; h += _node_size) {
        *(uint32_t *)h = i++;
    }

    uint32_t *last = (uint32_t *)(_nodes + _node_size * (_num_nodes - 1));
    *last = END_NUMBER;

    _first_free = 0;
}

PoolAllocator::~PoolAllocator() {
    // We don't own anything?
    if (_node_size == 0)
        return;

    // Free next allocator
    PoolAllocator *next_alloc = (PoolAllocator *)_mem;
    next_alloc->~PoolAllocator();

    // free(_mem);
    _backing->deallocate(_mem);

    // Set self to non-owning state, just for catching errors.
    this->_node_size = 0;
}

void *PoolAllocator::allocate(uint64_t size, uint64_t align) {
    assert(align <= 16);
    assert(size <= _node_size);

    if (_first_free == END_NUMBER) {
        // LOG_F(WARNING, "Pool Allocator '%s' is allocating a new pool", name());
        log_warn("Pool Allocator '%s' is allocating a new pool", name());

        PoolAllocator *next_alloc = (PoolAllocator *)_mem;
        if (next_alloc->_num_nodes == 0) {
            new (next_alloc) PoolAllocator(_node_size, _num_nodes, *_backing);
        }
        return next_alloc->allocate(size, align);
    } else {
        uint32_t *node = num_to_mem(this, _first_free);
        _first_free = *node;
        _nodes_allocated += 1;

        assert(uintptr_t(node) % align == 0);
        return (void *)node;
    }
}

void PoolAllocator::deallocate(void *p) {
    if (!p) {
        return;
    }

    char *nodes = (char *)fo::memory::align_forward(_mem + sizeof(PoolAllocator), 16u);
    char *end = nodes + _num_nodes * _node_size;
    if (nodes <= (char *)p && (char *)p < end) {
        // p is in range of this pool
        assert(((char *)p - nodes) % _node_size == 0);
        uint32_t node_num = ((char *)p - nodes) / _node_size;
        *(uint32_t *)p = _first_free;
        _first_free = node_num;
    } else {
        PoolAllocator *next_alloc = (PoolAllocator *)_mem;
        assert(next_alloc->_node_size != 0);
        next_alloc->deallocate(p);
    }
}

uint64_t PoolAllocator::total_allocated() {
    PoolAllocator *next_alloc = (PoolAllocator *)_mem;
    uint64_t t = _backing->total_allocated();
    if (t == Allocator::SIZE_NOT_TRACKED) {
        log_warn("PoolAllocator: %s has a backing allocator that returns SIZE_NOT_TRACKED", name());
        t = sizeof(PoolAllocator) + 16 + _node_size * _num_nodes;
    }
    if (next_alloc->_num_nodes == 0) {
        return t;
    }
    return t + next_alloc->total_allocated();
}

uint64_t PoolAllocator::allocated_size(void *p) {
    (void)p;
    return _node_size;
}

} // namespace fo
