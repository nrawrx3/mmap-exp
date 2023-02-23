#pragma once

#include <new>
#include <type_traits>

#include <scaffold/types.h>

#include <scaffold/debug.h>
#include <scaffold/memory_types.h>
#include <utility>

namespace fo {

static_assert(sizeof(void *) == 4 || sizeof(void *) == 8, "Just checking if 32 or 64 bit.");

// Represents the type (u32 or u64) that can hold the largest memory size on the platform. Could use size_t,
// but nah.
using AddrUint = typename std::conditional<sizeof(void *) == 8, uint64_t, uint32_t>::type;

#if !defined(PRIu64) || !defined(PRIu32)

#    error "..."

#endif

// For dealing with printf formats.
#if defined(SCAFFOLD_32_BIT)
#    define ADDRUINT_FMT "%" PRIu32

#else
#    define ADDRUINT_FMT "%" PRIu64
#endif

/// Base class for memory allocators.
///
/// Note: Regardless of which allocator is used, prefer to allocate memory in larger chunks instead of in many
/// small allocations. This helps with data locality, fragmentation, memory usage tracking, etc.
class SCAFFOLD_API Allocator {
  public:
    /// Default alignment for memory allocations.
    static constexpr uint64_t DEFAULT_ALIGN = alignof(void *);

    /// Maximum name size for any allocator including the '\0' character
    static constexpr uint64_t ALLOCATOR_NAME_SIZE = 32;

    static constexpr AddrUint DONT_CARE_OLD_SIZE = ~AddrUint(0);

    Allocator();

    // Dtor
    virtual ~Allocator() {}

    /// Allocates the specified amount of memory aligned to the specified alignment.
    virtual void *allocate(AddrUint size, AddrUint align = DEFAULT_ALIGN) = 0;

    /// Reallocate the region of memory. If nullptr, should treat as a call to `allocate`. If new_size is 0,
    /// should treat as a call to `deallocate`. `old_size` can be given when the user of the allocator keeps
    /// track of size themselves. This is mandatory when using an allocator that does not track size per
    /// allocation.
    virtual void *reallocate(void *old_allocation,
                             AddrUint new_size,
                             AddrUint align = DEFAULT_ALIGN,
                             AddrUint optional_old_size = DONT_CARE_OLD_SIZE) = 0;

    /// Frees an allocation previously made with allocate() or reallocate(). If `p` is nullptr, then simply
    /// returns doing nothing.
    virtual void deallocate(void *p) = 0;

    static constexpr uint64_t SIZE_NOT_TRACKED = ~uint64_t(0);

    /// Returns the amount of usable memory allocated at p. p must be a pointer returned by allocate() that
    /// has not yet been deallocated. The value returned will be at least the size specified to allocate(),
    /// but it can be bigger. (The allocator may round up the allocation to fit into a set of predefined slot
    /// sizes.)
    ///
    /// Not all allocators support tracking the size of individual allocations. An allocator that doesn't
    /// suppor it will return SIZE_NOT_TRACKED.

    virtual uint64_t allocated_size(void *p) = 0;

    /// Returns the total amount of memory allocated by this allocator. Note that the size returned can be
    /// bigger than the size of all individual allocations made, because the allocator may keep additional
    /// structures. If the allocator doesn't track memory, this function returns SIZE_NOT_TRACKED.
    virtual uint64_t total_allocated() = 0;

    /// Returns the name of this allocator. If no name was explicitly set by a call to `set_name` method,
    /// returns the `this` pointer stringified.
    const char *name() { return _name; }

    /// Sets the name of the allocator. The name must fit within `ALLOCATOR_NAME_SIZE` characters and must not
    /// be empty. `len` is the length of the string _including_ the '\0' character. Most allocators names
    /// should be known at compile time, so you can just use sizeof(literal) to obtain the size including the
    /// '\0'.
    void set_name(const char *name, uint64_t len = 0);

    /// Allocators cannot be copied.
    Allocator(const Allocator &other) = delete;
    Allocator &operator=(const Allocator &other) = delete;

  protected:
    struct DefaultReallocInfo {
        void *new_allocation;
        AddrUint size_difference;
        bool32 size_increased;
    };

    void default_realloc(void *old_allocation,
                         AddrUint new_size,
                         AddrUint align,
                         AddrUint old_size,
                         DefaultReallocInfo *out_info);

  private:
    /// The name of the allocator is stored in this array
    char _name[ALLOCATOR_NAME_SIZE];
};

/// Creates a new object of type T using the allocator a to allocate the memory. (don't use this)
#define MAKE_NEW(a, T, ...) (new ((a).allocate(sizeof(T), alignof(T))) T(__VA_ARGS__))

/// Frees an object allocated with MAKE_NEW. (don't use this either)
#define MAKE_DELETE(a, T, p)                                                                                 \
    do {                                                                                                     \
        if (p) {                                                                                             \
            (p)->~T();                                                                                       \
            a.deallocate(p);                                                                                 \
        }                                                                                                    \
    } while (0)

/// What the above macros do, but takes the arguments as template parameters. Better. Use this.
template <typename T, typename... Args> T *make_new(Allocator &a, Args &&...args) {
    return new (a.allocate(sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
}

/// Use this.
template <typename T> void make_delete(Allocator &a, T *object) {
    if (object) {
        object->~T();
        a.deallocate(object);
    }
}

/// Functions for accessing global memory data. See `memory.cpp` file for adding extra statically initialized
/// allocators.
namespace memory_globals {

struct InitConfig {
    // Size of the default scratch allocator's underlying buffer.
    uint64_t scratch_buffer_size = 4 * 1024;

    // Don't track leaks at all. This is a runtime option
    bool dont_track_malloc_leak = false;

    // If there is a 'leak', don't abort. Just notify with a print.
    bool dont_abort_if_leak = false;
};

/// Initializes the global memory allocators. scratch_buffer_size is the size of the memory buffer used by the
/// scratch allocators.
SCAFFOLD_API void init(const InitConfig &config = InitConfig());

/// Returns a default memory allocator that can be used for most allocations. You need to call init() for this
/// allocator to be available.
SCAFFOLD_API Allocator &default_allocator();

/// Returns a "scratch" allocator that can be used for temporary short-lived memory allocations. The scratch
/// allocator uses a ring buffer of size  scratch_buffer_size  to service the allocations. If there is not
/// enough memory in the buffer to match requests for scratch memory, memory from the default_allocator will
/// be returned instaed.
SCAFFOLD_API Allocator &default_scratch_allocator();

/// Shuts down the global memory allocators created by init().
SCAFFOLD_API void shutdown();
} // namespace memory_globals

namespace memory {
template <typename T> inline void *align_forward(void *p, T align);
inline void *pointer_add(void *p, uint64_t bytes);
inline const void *pointer_add(const void *p, uint64_t bytes);
inline void *pointer_sub(void *p, uint64_t bytes);
inline const void *pointer_sub(const void *p, uint64_t bytes);

// -- Inline implementations

// Aligns p to the specified alignment by moving it forward if necessary and returns the result.

template <typename T> inline void *align_forward(void *p, T align) {
    uintptr_t p_uint = uintptr_t(p);
    const uint64_t mod = p_uint % align;
    if (mod)
        p_uint += (align - mod);
    return (void *)p_uint;
}

/// Returns the result of advancing p by the specified number of bytes
inline void *pointer_add(void *p, uint64_t bytes) { return (void *)((char *)p + bytes); }

inline const void *pointer_add(const void *p, uint64_t bytes) {
    return (const void *)((const char *)p + bytes);
}

/// Returns the result of moving p back by the specified number of bytes
inline void *pointer_sub(void *p, uint64_t bytes) { return (void *)((char *)p - bytes); }

inline const void *pointer_sub(const void *p, uint64_t bytes) {
    return (const void *)((const char *)p - bytes);
}

} // namespace memory

} // namespace fo
