#include <scaffold/arena_allocator.h>
#include <scaffold/const_log.h>

namespace fo {

namespace arena_internal {

// This header is only used for supporting `allocated_size` method and debugging purposes. Contains the size
// of the allocated region and a
struct AllocationHeader {
    AddrUint previous_data_offset;
    AddrUint size;

    static constexpr AddrUint PADDING = std::numeric_limits<AddrUint>::max();
};

static_assert(sizeof(AllocationHeader) == 2 * sizeof(AddrUint), "");

}; // namespace arena_internal

using namespace arena_internal;

static inline AddrUint size_with_padding(AddrUint size, AddrUint align) {
    return sizeof(AllocationHeader) + align + size;
}

static inline void *data_pointer(AllocationHeader *header, AddrUint align) {
    AddrUint *p = reinterpret_cast<AddrUint *>(header + 1);
    return memory::align_forward(p, align);
}

static inline void fill_with_padding(void *data, AllocationHeader *header) {
    AddrUint *p = reinterpret_cast<AddrUint *>(header + 1);

    while (p < data) {
        *p = AllocationHeader::PADDING;
        ++p;
    }
}

ArenaAllocator::ArenaAllocator(Allocator &backing, AddrUint buffer_size)
    : _backing(&backing) {

    _buffer_size = buffer_size;

    // Size required to store a pointer header
    const AddrUint next_allocator_chunk_size =
        size_with_padding(sizeof(ArenaAllocator), alignof(ArenaAllocator));

    if (_buffer_size < next_allocator_chunk_size) {
        _buffer_size = next_allocator_chunk_size;

        log_warn(
            "Size of buffer managed by ArenaAllocator should be >= sizeof(ArenaAllocator) which is %zu bytes",
            sizeof(ArenaAllocator));
    }

    _mem = (u8 *)_backing->allocate(_buffer_size, alignof(AllocationHeader));

    AllocationHeader *h = reinterpret_cast<AllocationHeader *>(_mem);

    // Advance to the data pointer
    void *data = data_pointer(h, alignof(ArenaAllocator));
    fill_with_padding(data, h);

    h->size = sizeof(ArenaAllocator);
    h->previous_data_offset = (u8 *)data - _mem; // Doesn't matter

    // Create the uninitialized child ArenaAllocator at the head.
    new (data) ArenaAllocator();
    _child = reinterpret_cast<ArenaAllocator *>(data);

    _top = ((u8 *)data + sizeof(ArenaAllocator)) - _mem;
}

void *ArenaAllocator::allocate_no_lock(AddrUint size, AddrUint align) {
    if (size == 0) {
        return nullptr;
    }

    u8 *top_mem = _mem + _top;

    top_mem = (u8 *)memory::align_forward(top_mem, alignof(AllocationHeader));
    // ^ @rksht - This is not required usually if the user is making alignments of at least 4 always. See the
    // note on reallocate too.

    AddrUint total_chunk_size = size_with_padding(size, align);

    if (top_mem + total_chunk_size > end()) {
        set_full();
        return allocate_from_child(size, align);
    }

    AllocationHeader *h = reinterpret_cast<AllocationHeader *>(top_mem);

    void *data = data_pointer(h, align);
    fill_with_padding(data, h);

    h->size = size;
    h->previous_data_offset = _latest_allocation_offset;

    _latest_allocation_offset = (u8 *)data - _mem;

    _top = ((u8 *)data + size) - _mem;

    return data;
}

void *ArenaAllocator::allocate(AddrUint size, AddrUint align) {
    std::lock_guard<std::mutex> lk(_mutex);
    return allocate_no_lock(size, align);
}

void ArenaAllocator::deallocate(void *) {}

uint64_t ArenaAllocator::total_allocated() { return _buffer_size + (_child ? _child->total_allocated() : 0); }

AllocationHeader *header_before_data(u8 *data) {
    AddrUint *pad = (AddrUint *)(data);
    --pad;
    while (*pad == AllocationHeader::PADDING) {
        --pad;
    }
    AllocationHeader *h = (AllocationHeader *)(pad - 1);
    return h;
}

uint64_t ArenaAllocator::allocated_size_no_lock(void *p) {
    u8 *p8 = (u8 *)p;

    if (p8 < _mem || p8 >= end()) {
        if (_child->is_initialized()) {
            return _child->allocated_size(p);
        } else {
            log_assert(false, "Pointer %p not in range of ArenaAllocator", p);
        }
    }

#if 0
    AddrUint *pad = reinterpret_cast<AddrUint *>(p8);
    pad -= 1;
    while (*pad == AllocationHeader::PADDING) {
        --pad;
    }

    AllocationHeader *h = reinterpret_cast<AllocationHeader *>(pad - 1);
#endif
    AllocationHeader *h = header_before_data((u8 *)p);
    return h->size;
}

uint64_t ArenaAllocator::allocated_size(void *p) {
    std::lock_guard<std::mutex> lk(_mutex);

    return allocated_size_no_lock(p);
}

void *ArenaAllocator::reallocate(void *old_allocation, AddrUint new_data_size, AddrUint align, AddrUint old_size) {
    (void)old_size; // This allocator tracks size per allocation

    if (old_allocation == nullptr) {
        return allocate(new_data_size, align);
    }

    std::lock_guard<std::mutex> lk(_mutex);

    // Reallocate from child buffer if pointer is not into this one's range.

    u8 *const old8 = (u8 *)old_allocation;

    if (old8 < _mem || old8 >= _mem + _buffer_size) {
        log_assert(_child != nullptr,
                   "old_allocation(= %p) seems to be an invalid pointer, searched all children buffers.", old8);
        return _child->reallocate(old_allocation, new_data_size, align);
    }

    AllocationHeader *old_header = header_before_data(old8);

    // Check if it can be extended
    u32 old_data_size = old_header->size;

    // Handle the case where the user wants to shrink the space. No such thang in this allocator boy... Or
    // maybe we could shrink space if this is the last allocation. @rksht: Later perhaps.
    if (new_data_size < old_data_size) {
        return old_allocation;
    }

    AddrUint old_offset = AddrUint(old8 - _mem);

    // Check if this can be extended.
    if (old_offset == _latest_allocation_offset) {
        // This is the last allocation. Check if there's enough space.

        AddrUint remaining_bytes = _buffer_size - old_offset;

        if (remaining_bytes >= new_data_size) {
            old_header->size = new_data_size;
            _top = (old8 + new_data_size) - _mem;

            log_info("Extended old allocation at (%lu) from %lu bytes to %lu bytes",
                     (ulong)old_offset,
                     (ulong)old_data_size,
                     (ulong)new_data_size);

            return old8;
        }

        // Could not extend. Just allocate a block from a child buffer, and copy the old data.
        u8 *new_allocation = (u8 *)allocate_from_child(new_data_size, align);
        memcpy(new_allocation, old8, old_data_size);

        // But still, old_allocation is the last allocation nonetheless. We can reduce the top pointer to
        // point to the end of the previous allocation. Note that we cannot simply set it to `old_header`'s
        // address, since it might be that there was some alignment padding for it. This is quite rare. You
        // wouldn't use an ArenaAllocator to allocate non-power-of-2 blocks, and header is only 4 bytes
        // aligned, that's the minimum alignment.

        // _top = (u8 *)old_header - _mem;
        auto previous_header = header_before_data(_mem + old_header->previous_data_offset);
        _top = old_header->previous_data_offset + previous_header->size;
        _latest_allocation_offset = old_offset;

        log_info("Could not extend old allocation (%lu). But freed up the tail due to it being the latest "
                 "allocation",
                 (ulong)old_offset);

        return new_allocation;

    } else {
        // Not the last allocation. Do the brute thing.
        void *new_allocation = allocate_no_lock(new_data_size, align);
        memcpy(new_allocation, old_allocation, old_data_size);

        log_info("Realloc of (%lu) created hole", (ulong)old_offset);

        return new_allocation;
    }
}

void *ArenaAllocator::allocate_from_child(AddrUint size, AddrUint align) {
    // Create a child allocator if there isn't one.

    if (!_child->is_initialized()) {
        AddrUint child_buffer_size_needed =
            sizeof(AllocationHeader) + alignof(ArenaAllocator) + sizeof(ArenaAllocator) + align + size;

        // If the size of allocation is more than 4 times the initial buffer size, fail.
        AddrUint multiple = ceil_div(child_buffer_size_needed, _buffer_size);

        if (multiple > 4) {
            log_warn("Arena Allocator %s of buffer size = " ADDRUINT_FMT
                     "bytes requested allocation of size" ADDRUINT_FMT "bytes",
                     name(),
                     _buffer_size,
                     size);
        }

        child_buffer_size_needed = clip_to_pow2(child_buffer_size_needed);

        // Double the buffer for new allocation if that option is enabled.
        if ((_options & 0x2) && child_buffer_size_needed < 2 * _buffer_size) {
            child_buffer_size_needed = 2 * _buffer_size;
        }

        log_info("ArenaAllocator of size %.2f KB  allocating a child buffer of size %.2f KB",
                 _buffer_size / 1024.0,
                 child_buffer_size_needed / 1024.0);

        // Delete the empty child allocator and create a new ArenaAllocator. This deletion could be skipped,
        // but it's undefined behavior to overwrite an object
        _child->~ArenaAllocator();
        new (_child) ArenaAllocator(*_backing, child_buffer_size_needed);
    }

    return _child->allocate(size, align);
}

ArenaAllocator::~ArenaAllocator() {
    if (_child) {
        _child->~ArenaAllocator();
        _child = nullptr;
    }

    if (_mem) {
        _backing->deallocate(_mem);
        _buffer_size = 0;
        _top = 0;
        _mem = nullptr;
        _options = 0;
    }
}

void ArenaAllocator::get_chain_info(fo::Array<ArenaInfo> &a) const {
    std::lock_guard<std::mutex> lk(_mutex);

    ArenaInfo info;
    info.buffer_size = _buffer_size;
    info.total_allocated = _top;

    push_back(a, info);

    if (_child) {
        _child->get_chain_info(a);
    }
}

void ArenaAllocator::set_full() {
    if (!(_options & 0x1)) {
        log_info("ArenaAllocator - %s full. Allocating child", name());
    }
    _options |= 0x1;
}
void ArenaAllocator::set_mul_by_2() { _options |= 0x2; }

#if 0
void ArenaAllocator::set_allow_child_buffer(bool allow) {

    _options |= (u32(allow) << 3);
}
#endif

} // namespace fo
