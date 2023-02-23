#pragma once

#include <scaffold/debug.h>
#include <scaffold/memory.h>

#include <cstdint>

namespace fo {

struct SCAFFOLD_API PoolAllocator;

/// Currently only returns allocations aligned to 16 bytes. Pools are just buffers total ussable size
/// `_node_size * _num_nodes`, and each allocation returns a free node of size `_node_size`. If pool is
/// exhausted, the backing allocator provided will be used to create another pool of the same size. This will
/// go on indefinitely as more and more allocations are done, without deallocations.
struct alignas(16) PoolAllocator : public Allocator {
    uint32_t _node_size; // = 0 implies allocator doesn't own any buffer
    uint32_t _num_nodes;
    uint32_t _nodes_allocated;
    uint32_t _first_free;
    char *_mem;
    Allocator *_backing;

  private:
    PoolAllocator(); // Private because this constructs a non-owning (useless) allocator

  public:
    // Creates a new pool allocator that uses the `backing` allocator to allocate the buffer it manages.
    PoolAllocator(uint64_t node_size,
                  uint64_t num_nodes,
                  Allocator &backing = memory_globals::default_allocator());

    virtual ~PoolAllocator();

    PoolAllocator(const PoolAllocator &other) = delete; // Nope

    virtual void *allocate(uint64_t size, uint64_t align = fo::Allocator::DEFAULT_ALIGN) override;
    virtual void deallocate(void *p) override;

    // Returns the total amount of memory allocated (not just the total number of currently allocated nodes).
    virtual uint64_t total_allocated() override;

    // Always returns `_node_size`. Doesn't check if `p` is valid.
    virtual uint64_t allocated_size(void *p) override;

    virtual void *reallocate(void *, AddrUint, AddrUint, AddrUint old_size = DONT_CARE_OLD_SIZE) override {
        log_assert(false, "PoolAllocator does not support reallocate()");
        (void)old_size;
        return nullptr;
    }
};

} // namespace fo
