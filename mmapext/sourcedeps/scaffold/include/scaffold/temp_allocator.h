#pragma once

#include <scaffold/memory.h>

namespace fo {
/// A temporary memory allocator that primarily allocates memory from a local stack buffer of size
/// BUFFER_SIZE. If that memory is exhausted it will use the backing allocator (typically a scratch
/// allocator).
///
/// Memory allocated with a TempAllocator does not have to be deallocated. It is automatically deallocated
/// when the TempAllocator is destroyed.  It's easiest to this allocator as a local variable, tying its
/// lifetime to a scope. Otherwise, if you create this allocator on the heap, or global storage make sure to
/// call the destructor after all objects using it are dead.

struct TempAllocatorConfig {
    fo::Allocator *backing_allocator = &fo::memory_globals::default_allocator();

    unsigned chunk_size = 4 * 1024; // Chunks to allocate from backing allocator
    bool log_on_exhaustion = false;
    const char *name = nullptr;

    TempAllocatorConfig() = default;

    constexpr TempAllocatorConfig(fo::Allocator *backing_allocator,
                                  unsigned chunk_size,
                                  bool log_on_exhaustion,
                                  const char *name)
        : backing_allocator(backing_allocator)
        , chunk_size(chunk_size)
        , log_on_exhaustion(log_on_exhaustion)
        , name(name) {}

    TempAllocatorConfig(fo::Allocator &backing_allocator)
        : backing_allocator(&backing_allocator) {}

    // Returns a temp allocator config that will not try to use any backing allocator and return nullptr if
    // local storage runs out. One advantange of this is that you can create TempAllocators that don't depend
    // on memory_globals::init and memory_globals::shutdown. So you basically will see TempAllocators as arena
    // allocators managing static arrays.
    static constexpr TempAllocatorConfig
    local_only(bool log_on_exhaustion = true, unsigned chunk_size = 4 * 1024, const char *name = nullptr) {
        return TempAllocatorConfig{ nullptr, chunk_size, log_on_exhaustion, name };
    }
};

template <int BUFFER_SIZE> class TempAllocator : public Allocator {
  public:
    /// Creates a new temporary allocator using the specified backing allocator.
    TempAllocator(const TempAllocatorConfig &config = TempAllocatorConfig());
    virtual ~TempAllocator();

    void *allocate(AddrUint size, AddrUint align = DEFAULT_ALIGN) override;

    void *
    reallocate(void *old_allocation, AddrUint new_size, AddrUint align, AddrUint must_old_size) override {
        DefaultReallocInfo realloc_info = {};
        default_realloc(old_allocation, new_size, align, must_old_size, &realloc_info);
        return realloc_info.new_allocation;
    }

    /// Deallocation is a NOP for the TempAllocator. The memory is automatically
    /// deallocated when the TempAllocator is destroyed.
    void deallocate(void *) override {}

    /// Returns SIZE_NOT_TRACKED.
    AddrUint allocated_size(void *) override { return SIZE_NOT_TRACKED; }

    /// Returns SIZE_NOT_TRACKED.
    AddrUint total_allocated() override { return SIZE_NOT_TRACKED; }

  private:
    char _buffer[BUFFER_SIZE]; //< Local stack buffer for allocations.
    Allocator *_backing;       //< Backing allocator if local memory is exhausted.
    char *_start;              //< Start of current allocation region
    char *_p;                  //< Current allocation pointer.
    char *_end;                //< End of current allocation region
    unsigned _chunk_size;      //< Chunks to allocate from backing allocator
    int _first_time_exhausted; //< 0 => Don't log on exhausttion. 1 => Not exhausted, 2 => Just exhausted
};

// If possible, use one of these predefined sizes for the TempAllocator to avoid
// unnecessary template instantiation.
typedef TempAllocator<64> TempAllocator64;
typedef TempAllocator<128> TempAllocator128;
typedef TempAllocator<256> TempAllocator256;
typedef TempAllocator<512> TempAllocator512;
typedef TempAllocator<1024> TempAllocator1024;
typedef TempAllocator<2048> TempAllocator2048;
typedef TempAllocator<4096> TempAllocator4096;

// ---------------------------------------------------------------
// Inline function implementations
// ---------------------------------------------------------------

template <int BUFFER_SIZE>
TempAllocator<BUFFER_SIZE>::TempAllocator(const TempAllocatorConfig &config)
    : _backing(config.backing_allocator)
    , _chunk_size(config.chunk_size)
    , _first_time_exhausted((int)config.log_on_exhaustion) {
    _p = _start = _buffer;
    _end = _start + BUFFER_SIZE;
    *(void **)_start = 0;
    _p += sizeof(void *);

    if (config.name) {
        set_name(config.name);
    }
}

template <int BUFFER_SIZE> TempAllocator<BUFFER_SIZE>::~TempAllocator() {
    if (_backing == nullptr) {
        return;
    }
    // Using this variable to prevent string-aliasing warnings. Looking at
    // _buffer as if it's an array of (char *)s

    char **p_buffer = (char **)_buffer;
    char *p = p_buffer[0];
    while (p) {
        char *next = *(char **)p;
        _backing->deallocate(p);
        p = next;
    }
}

template <int BUFFER_SIZE> void *TempAllocator<BUFFER_SIZE>::allocate(AddrUint size, AddrUint align) {
    _p = (char *)memory::align_forward(_p, align);
    if ((int)size > _end - _p) {

        if (_backing == nullptr) {
            log_info("TempAllocator - '%s' ran out of local storage and has no backing allocator given.  "
                     "(chunk_size = %u)",
                     name(),
                     _chunk_size);
            return nullptr;
        }

        // Sizeof next pointer + requested + aligment (safe estimate)
        AddrUint to_allocate = sizeof(void *) + size + align;
        if (to_allocate < _chunk_size)
            to_allocate = _chunk_size;
        _chunk_size *= 2;
        void *p = _backing->allocate(to_allocate);
        *(void **)_start = p;
        _p = _start = (char *)p;
        _end = _start + to_allocate;
        *(void **)_start = 0;
        _p += sizeof(void *);
        memory::align_forward(p, align);

        if (_first_time_exhausted == 1) {
            log_info("TempAllocator - '%s' allocated from backing allocator (chunk_size = %u)",
                     name(),
                     _chunk_size);
            _first_time_exhausted = 2;
        }
    }

    void *result = _p;
    _p += size;
    return result;
}
} // namespace fo
