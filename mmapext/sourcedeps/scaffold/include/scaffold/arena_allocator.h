#pragma once

#include <limits>
#include <scaffold/array.h>
#include <scaffold/debug.h>
#include <scaffold/memory.h>

#include <mutex>

namespace fo {

/// Struct containing information for a particular arena. The `get_chain_info` method returns an array of
/// these structures, one for each arena in a chain.
struct ArenaInfo {
    AddrUint buffer_size;
    AddrUint total_allocated;
};

/// An ArenaAllocator. Allocations must be aligned to at least 16 bytes and are simply done by incrementing a
/// pointer (and then some alignment). It does not support deallocating previous allocations individually.
/// Instead it will deallocate all the memory it owns when it's destroyed.
class SCAFFOLD_API ArenaAllocator : public Allocator {
  private:
    mutable std::mutex _mutex;

    // The allocator from where the arenas are allocated from.
    Allocator *_backing = nullptr;

    // Allocated buffer
    u8 *_mem = nullptr;

    // In case the current buffer is exhausted, and the option is enabled, a child allocator can be
    // initialized to manager another buffer. This forms a chain of ArenaAllocators.
    ArenaAllocator *_child = nullptr;

    // Size in bytes of the buffer allocated from `_backing`
    AddrUint _buffer_size = 0;

    // Offset to the top memory where the next allocation will be done at.
    AddrUint _top = 0;

    // Offset to the last allocation. Used for the reallocate operation.
    AddrUint _latest_allocation_offset = 0;

    // Extra options stored as a u32. Can configure the allocator, see the related functions.
    u32 _options = 0;

  private:
    // Constructs an unitialized allocator stored at the head of the buffer.
    ArenaAllocator() = default;

    void *allocate_no_lock(AddrUint size, AddrUint align);

    void *allocate_from_child(AddrUint size, AddrUint align);

    uint64_t allocated_size_no_lock(void *p);

    u8 *end() const { return _mem + _buffer_size; }

    void set_full();

    bool is_initialized() const { return _mem != nullptr; }

  public:
    /// Create an arena allocator. The internal buffer that this arena allocator owns will be allocated using
    /// given backing allocator. The size of the buffer will be `buffer_size`. If this buffer gets full in the
    /// future, another will be allocated using the same `backing` allocator.
    ArenaAllocator(Allocator &backing, AddrUint buffer_size);

    ~ArenaAllocator();

    void *allocate(AddrUint size, AddrUint align = DEFAULT_ALIGN) override;

    void *reallocate(void *old_allocation,
                     AddrUint new_size,
                     AddrUint align = DEFAULT_ALIGN,
                     AddrUint old_size = DONT_CARE_OLD_SIZE) override;

    void deallocate(void *p) override;

    uint64_t allocated_size(void *p) override;

    uint64_t total_allocated() override;

    void get_chain_info(fo::Array<ArenaInfo> &a) const;

    // Child buffer will allocate twice the size of current buffer.
    void set_mul_by_2();

    // Child buffers will be allocated if an allocation cannot be made from current buffer. This is the
    // default behavior. Disable/Re-enable it by calling this.
    void set_allow_child_buffer(bool allow);
};

} // namespace fo
