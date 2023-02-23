// TODO - messy. clean up. make reallocate thread-safe.

// For posix_memalign
#if MALLOC_ALLOCATOR_DONT_TRACK_SIZE
#    if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE < 200112L)
#        define _POSIX_C_SOURCE 200112L
#    endif
#endif

#include <scaffold/const_log.h>
#include <scaffold/debug.h>
#include <scaffold/memory.h>

#include <assert.h>
#include <mutex>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

namespace fo {

Allocator::Allocator() { strcpy(_name, "<Unnamed>"); };

void Allocator::set_name(const char *name, uint64_t len) {
    if (len == 0) {
        len = (uint64_t)strlen(name);
    }
    assert(len < ALLOCATOR_NAME_SIZE && "Allocator name too large");
    memcpy(_name, name, len);
}

void Allocator::default_realloc(void *old_allocation,
                                AddrUint new_size,
                                AddrUint align,
                                AddrUint old_size,
                                DefaultReallocInfo *out_info) {
    // TODO: The base implementation doesn't lock a mutex! Best to just not use the base implementation of
    // reallocate.

    if (old_allocation == nullptr) {
        out_info->new_allocation = allocate(new_size, align);
        out_info->size_difference = new_size;
        out_info->size_increased = true;

        return;
    }

    if (new_size == 0) {
        deallocate(old_allocation);
        out_info->new_allocation = nullptr;
        out_info->size_difference = new_size;
        out_info->size_increased = false;

        return;
    }

    if (old_size == DONT_CARE_OLD_SIZE) {
        old_size = (AddrUint)allocated_size(old_allocation);
        assert(
            old_size != SIZE_NOT_TRACKED &&
            "Default reallocate only works for Allocator implementations that never return SIZE_NOT_TRACKED");
    }

    if (old_size == new_size) {
        out_info->new_allocation = old_allocation;
        out_info->size_increased = false;
        out_info->size_difference = 0;
        return;
    }

    out_info->new_allocation = allocate(new_size, align);

    if (old_size < new_size) {
        out_info->size_increased = true;
        out_info->size_difference = new_size - old_size;
    } else {
        out_info->size_increased = false;
        out_info->size_difference = old_size - new_size;
    }

    memcpy(out_info->new_allocation, old_allocation, out_info->size_increased ? old_size : new_size);
    deallocate(old_allocation);
}

} // namespace fo

namespace {

using namespace fo;

template <typename SizeType> struct Header {
    SizeType size;

    // Allocations contain a memory region allocated like <Header> <Pad>* <UsableChunk>. We want to find the
    // header given a pointer to the UsableChunk. Since UsableChunk cannot be the max value of a uint32_t or
    // uint64_t, depending on the platform, we can use that as the pad values.
    static constexpr SizeType pad_value = std::numeric_limits<SizeType>::max();
    static constexpr SizeType alignment = alignof(SizeType);
    using size_type = SizeType;
};

using Header32 = Header<u32>;
using Header64 = Header<u64>;
using HeaderNative = Header<AddrUint>;

// Given a pointer to the header, returns a pointer to the data that immediately follows it.
template <typename HeaderType> inline void *data_pointer(HeaderType *header, uint32_t align) {
    void *p = header + 1;
    return memory::align_forward(p, align);
}

// Given a pointer to the data, returns a pointer to the header before it.
template <typename HeaderType> inline HeaderType *header_before_data(void *data) {
    auto *p = reinterpret_cast<typename HeaderType::size_type *>(data);
    while (p[-1] == HeaderType::pad_value) {
        --p;
    }
    return reinterpret_cast<HeaderType *>(p) - 1;
}

// Stores the size in the header and pads with pad_value up to the data pointer.
template <typename HeaderType>
inline void fill_with_padding(HeaderType *header, void *data, typename HeaderType::size_type size) {
    using size_type = typename HeaderType::size_type;

    header->size = static_cast<size_type>(size);

    size_type *p = reinterpret_cast<size_type *>(header + 1);

    while (p < data) {
        *p++ = HeaderType::pad_value;
    }
}

/// An allocator that uses the default system malloc(). Allocations are padded so that we can store the size
/// of each allocation and align them to the desired alignment.
///
/// Note: An OS-specific allocator that can do alignment and tracks size does need this padding and can thus
/// be more efficient than the MallocAllocator
class MallocAllocator : public Allocator {
    AddrUint _total_allocated;

    std::mutex _mutex;

    // TODO: rksht - Not implemented. Tracking is set as a compile time option.
    bool _tracking;

    // Returns the size to allocate from malloc() for a given size and align.
    static inline AddrUint size_with_padding(AddrUint size, AddrUint align) {
        return size + align + sizeof(HeaderNative);
    }

  public:
    MallocAllocator(bool tracking = true)
        : _total_allocated(0)
        , _mutex{}
        , _tracking(tracking) {
            
        (void)_tracking; // Unused private field.
#if MALLOC_ALLOCATOR_DONT_TRACK_SIZE
        _total_allocated = SIZE_NOT_TRACKED;
#endif
    }

    ~MallocAllocator() {
        // Check that we don't have any memory leaks when allocator is destroyed.
#if MALLOC_ALLOCATOR_DONT_TRACK_SIZE == 0
        if (_total_allocated != 0) {
            log_err("MallocAllocator %s some memory still not deallocated - _total_allocated = " ADDRUINT_FMT
                    "\n",
                    name(),
                    _total_allocated);
        }
        // assert(_total_allocated == 0);
#endif
    }

    uint64_t total_allocated() override {
        std::lock_guard<std::mutex> lk(_mutex);
        return _total_allocated;
    }

#if MALLOC_ALLOCATOR_DONT_TRACK_SIZE
    void *allocate(AddrUint size, AddrUint align) override {
        std::lock_guard<std::mutex> lk(_mutex);

        void *p = nullptr;
#    ifdef WIN32
        p = _aligned_malloc(size, align);
#    else
        int ret;

        // The minimum alignment required for posix_memalign
        if (align % alignof(void *) != 0) {
            align = alignof(void *);
        }

        if ((ret = posix_memalign(&p, align, size)) != 0) {
            log_err("MallocAllocator failed to allocate - error = %s, align = " ADDRUINT_FMT,
                    ret == EINVAL ? "EINVAL" : "ENOMEM",
                    align);
            abort();
        }
#    endif
        return p;
    }

    void deallocate(void *p) override {
        // Don't need to lock
#    ifdef WIN32
        _aligned_free(p);
#    else
        free(p);
#    endif
    }

    void *
    reallocate(void *old_allocation, AddrUint new_size, AddrUint align, AddrUint maybe_old_size) override {
#    ifdef WIN32
        void *new_allocation = _aligned_realloc(old_allocation, new_size, align);

#    else
        // There is no posix_memalign_realloc or something like that on posix. But usually realloc will
        // allocated with enough alignment for most `align` values we will use. So we try to realloc first
        // plainly. If the returned address is not aligned adequately, we will revert to
        // posix_memalign'ing a new block, copying old data, deallocating old memory.

        align = align < alignof(void *) ? alignof(void *) : align;

        void *new_allocation = realloc(old_allocation, new_size);

        if (AddrUint(new_allocation) % align != 0) {
            log_warn("This is slow. realloc did not allocate with an alignment of %lu, requires double copy",
                     (long unsigned int)align);

            void *new_new = allocate(new_size, align);
            memcpy(new_new, new_allocation, new_size);
            free(new_allocation);
            new_allocation = new_new;
        }

#    endif

        return new_allocation;
    }

    uint64_t allocated_size(void *p) override {
        (void)p;
        return SIZE_NOT_TRACKED;
    }
#else
    void *allocate(AddrUint size, AddrUint align) override {
        std::lock_guard<std::mutex> lk(_mutex);

        const AddrUint ts = size_with_padding(size, align);

        HeaderNative *h = reinterpret_cast<HeaderNative *>(malloc(ts));

        void *p = data_pointer(h, align);
        fill_with_padding(h, p, size);
        // ^Unlike the original version, we store the requested size, not plus the padding.

        _total_allocated += size;
        assert((uintptr_t)p % align == 0);

#    if 0
        log_info("allocate called with size: %lu - ptr = %p", (ulong)size, p);
#    endif

        return p;
    }

    void *reallocate(void *old_allocation,
                     AddrUint new_size,
                     AddrUint align,
                     AddrUint optional_old_size = DONT_CARE_OLD_SIZE) override {
        DefaultReallocInfo realloc_info = {};
        default_realloc(old_allocation, new_size, align, optional_old_size, &realloc_info);

        return realloc_info.new_allocation;
    }

    void deallocate(void *p) override {
        std::lock_guard<std::mutex> lk(_mutex);

        if (!p) {
            return;
        }

        HeaderNative *h = header_before_data<HeaderNative>(p);
        _total_allocated -= h->size;
        free(h);
    }

    uint64_t allocated_size(void *p) override {
        // No need to lock since if some other thread is deallocating the memory region, there is already a
        // data race on the user side. Just saying.
        return header_before_data<HeaderNative>(p)->size;
    }
#endif
};

/// An allocator used to allocate temporary "scratch" memory. The allocator uses a fixed size ring buffer to
/// services the requests.
///
/// Memory is always always allocated linearly. An allocation pointer is  advanced through the buffer as
/// memory is allocated and wraps around at  the end of the buffer. Similarly, a free pointer is advanced as
/// memory  is freed.
///
/// It is important that the scratch allocator is only used for short-lived memory allocations. A long lived
/// allocator will lock the "head" pointer and prevent the "tail" pointer from proceeding past it, which
/// means the ring buffer can't be used. If possible, do large allocations, as opposed to small ones, as
/// that would waste extra space due to each allocation also requiring allocating a 4 or 8 byte header.
///
/// If the ring buffer is exhausted, the scratch allocator will use its backing allocator to allocate memory
/// instead.
class ScratchAllocator : public Allocator {
    Allocator &_backing;

    std::mutex _mutex;

    u8 *_begin; // Start of the whole underlying buffer
    u8 *_end;   // End of the whole underlying buffer
    u8 *_tail;  // Allocations happen starting with this address. Aligned to 4 bytes always
    u8 *_head;  // Allocations cannot proceed past this pointer

    // The header of free blocks have the `size` field's msb set to 1.
    static constexpr u32 FREE_BLOCK_MASK = u32(1) << 31;

    // Just a check
    static_assert(alignof(Header32) == 4, "");

    // Returns true if the addresss p is within the allocable range
    bool _in_use(void *p) {
        // Happens when the scratch buffer capacity is altogether too small for allocation.
        if (_head == _tail) {
            return p >= _end;
        }

        // Case when there's no wrap-around in the buffer.
        if (_tail > _head) {
            return p >= _head && p < _tail;
        }

        // Case when the allocations currently wrapped around.
        return p < _tail || p >= _head;
    }

  public:
    /// Creates a ScratchAllocator. The allocator will use the backing allocator to create the ring buffer
    /// and to service any requests that don't fit in the ring buffer.
    ///
    /// size specifies the capacity of the ring buffer.
    ScratchAllocator(Allocator &backing, uint64_t size)
        : _backing(backing)
        , _mutex{} {
        // Increase size to multiple of 4 if it isn't already.
        size = ((size + 3) / 4) * 4;
        _begin = (u8 *)_backing.allocate(size, 16);
        _end = _begin + size;
        _tail = _begin;
        _head = _begin;
        // memset(_begin, 200, size);
    }

    ~ScratchAllocator() {
        assert(_head == _tail);
        _backing.deallocate(_begin);
    }

    void *allocate(uint64_t size, uint64_t align) override {
        std::lock_guard<std::mutex> lk(_mutex);

        // Guard against shenanigans.
        if (size == 0) {
            return nullptr;
        }

        // Ensures the _tail pointer will be pointing to a multiple of 4 address after allocation.
        assert(align % 4 == 0);

        u8 *p = _tail;
        Header32 *h = (Header32 *)p;
        u8 *data = (u8 *)data_pointer(h, (u32)align);

        // Round up the size to a multiple of alignment
        size = ((size + 3) / 4) * 4;

        p = data + size;

        // Reached the end of the buffer.
        if (p > _end) {
            // Should hold since capacity is a multiple of 4
            assert((u8 *)h <= _end);

            // If there is an unusable portion at the tail, mark it as a free block.
            if ((u8 *)h != _end) {
                h->size = u32(_end - (u8 *)h);
                h->size |= FREE_BLOCK_MASK;
            }

            p = _begin;
            h = (Header32 *)_begin;

            data = (u8 *)data_pointer(h, align);
            p = data + (u32)size;
        }

        // If the buffer is exhausted use the backing allocator instead.
        if (_in_use(p)) {
            log_info("ScratchAllocator - %s, using backing allocator", name());
            return _backing.allocate(size, align);
        }

        fill_with_padding(h, data, p - (u8 *)h);
        _tail = p;

        // log_info("ScratchAlloctor - %s - Allocated %u bytes - pointer - %p ", name(), (u32)size, data);

        return data;
    }

    void deallocate(void *p) override {
        std::lock_guard<std::mutex> lk(_mutex);

#if 0
        // Had to actually fix this class. Keeping the log-info.
        {
            auto s = p ? std::to_string(allocated_size(p)) : "<null>";
            log_info("Deallocated - %s bytes - pointer - %p", s.c_str(), p);
        }
#endif

        if (!p) {
            return;
        }

        if (p < _begin || p >= _end) {
            _backing.deallocate(p);
            return;
        }

        // Mark this slot as free
        Header32 *h = header_before_data<Header32>(p);
        assert((h->size & FREE_BLOCK_MASK) == 0);
        h->size |= FREE_BLOCK_MASK;

        // Advance the free pointer past all free slots.
        while (_head != _tail) {
            Header32 *h = (Header32 *)_head;
            if ((h->size & FREE_BLOCK_MASK) == 0) {
                break;
            }

            _head += (h->size & ~FREE_BLOCK_MASK);
            if (_head == _end) {
                _head = _begin;
            }
        }
    }

    uint64_t allocated_size(void *p) override {
        Header32 *h = header_before_data<Header32>(p);
        return h->size - ((char *)p - (char *)h);
    }

    uint64_t total_allocated() override {
        std::lock_guard<std::mutex> lk(_mutex);
        return _end - _begin;
    }

    void *reallocate(void *old_allocation, AddrUint new_size, AddrUint align, AddrUint optional_old_size) override {
        // Don't think it's worth it to implement reallocate to handle growing the tail if old_allocation is
        // at the tail
        DefaultReallocInfo realloc_info = {};
        default_realloc(old_allocation, new_size, align, optional_old_size, &realloc_info);
        return realloc_info.new_allocation;
    }
};

/// This struct should contain all allocators required by the application/library/deathray etc. Put any extra
/// global allocators required inside this struct.
struct MemoryGlobals {
    alignas(MallocAllocator) char _default_allocator[sizeof(MallocAllocator)];
    alignas(ScratchAllocator) char _default_scratch_allocator[sizeof(ScratchAllocator)];

    // Don't like to have strict aliasing related warnings. I can spare a few
    // bytes to point to the above two chunks
    MallocAllocator *default_allocator;
    ScratchAllocator *default_scratch_allocator;
};

MemoryGlobals _memory_globals;
} // namespace

namespace fo {

namespace memory_globals {

/// Allocator names should be defined here statically...
static const char default_allocator_name[] = "default_alloc";
static const char default_scratch_allocator_name[] = "default_scratch_alloc";

/// ... And add the initialization code here ...
void init(const InitConfig &config) {
    _memory_globals.default_allocator = new (_memory_globals._default_allocator) MallocAllocator{};
    _memory_globals.default_scratch_allocator = new (_memory_globals._default_scratch_allocator)
        ScratchAllocator{ *(MallocAllocator *)_memory_globals.default_allocator, config.scratch_buffer_size };

    default_allocator().set_name(default_allocator_name, sizeof(default_allocator_name));
    default_scratch_allocator().set_name(default_scratch_allocator_name,
                                         sizeof(default_scratch_allocator_name));
}

Allocator &default_allocator() { return *_memory_globals.default_allocator; }

Allocator &default_scratch_allocator() { return *_memory_globals.default_scratch_allocator; }

/// ... And add deallocation code here.
void shutdown() {
    _memory_globals.default_scratch_allocator->~ScratchAllocator();
    // MallocAllocator must be last as its used as the backing allocator for
    // others
    _memory_globals.default_allocator->~MallocAllocator();
    _memory_globals = MemoryGlobals{};
}

} // namespace memory_globals
} // namespace fo
