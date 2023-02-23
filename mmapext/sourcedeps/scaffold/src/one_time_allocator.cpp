#include <scaffold/one_time_allocator.h>

#include <assert.h>
#include <inttypes.h>
#include <limits>

#if __has_include(<sys/mman.h>)
#    define MMAP_AVAILABLE 1
#    include <sys/mman.h>
#else
#    define MMAP_AVAILABLE 0
#endif

// Keeping the state information in the `_total_allocated` member itself, which = 0 denotes initialized, =
// ~AddrUint(0) denotes deallocated, otherwise it means memory is currenlty allocated.

namespace fo {

OneTimeAllocator::OneTimeAllocator()
    : _total_allocated(0)
    , _mem(nullptr) {}

OneTimeAllocator::~OneTimeAllocator() {
    log_assert(_total_allocated == std::numeric_limits<AddrUint>::max(),
               "%s - Memory is still allocated",
               __PRETTY_FUNCTION__);
}

void *OneTimeAllocator::allocate(AddrUint size, AddrUint align) {
    assert(size != 0 && "Size must not be 0");

    (void)align;

#if MMAP_AVAILABLE
    // mmap aligns to page frame size, which is always enough, so ignore.
    _mem = mmap(nullptr, (size_t)size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
#else

    _mem = memory_globals::default_allocator().allocate(size, align);

#endif

    log_assert(
        _mem != nullptr, "%s - Failed to allocate size = " ADDRUINT_FMT " bytes", __PRETTY_FUNCTION__, size);
    _total_allocated = size;

    return _mem;
}

void OneTimeAllocator::deallocate(void *p) {
    log_assert(p == _mem, "Pointer to deallocate is not the same one that got allocated!");
    log_assert(_total_allocated != 0 && _total_allocated != std::numeric_limits<AddrUint>::max(),
               "Either unallocated or tried to deallocated twice");

#if MMAP_AVAILABLE
    int res = munmap(p, (size_t)_total_allocated);
    log_assert(res == 0, "%s - munmap failed", __PRETTY_FUNCTION__);

#else
    memory_globals::default_allocator().deallocate(_mem);

#endif
    _mem = nullptr;
    _total_allocated = std::numeric_limits<AddrUint>::max();
}

void *OneTimeAllocator::reallocate(void *, AddrUint, AddrUint, AddrUint) {
    log_assert(false, "OneiTimeAllocator('%s) does not support reallocate()", name());
    return nullptr;
}

AddrUint OneTimeAllocator::total_allocated() { return _total_allocated; }

AddrUint OneTimeAllocator::allocated_size(void *p) {
    (void)p;
    assert(_mem == p);
    return _total_allocated;
}

} // namespace fo
