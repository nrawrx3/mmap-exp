#include <scaffold/memory.h>

namespace fo {

class OneTimeAllocator : public Allocator {
  public:
    OneTimeAllocator();
    virtual ~OneTimeAllocator();

    /// Allocates memory using mmap. Will fail if called more than once.
    virtual void *allocate(AddrUint size, AddrUint align = Allocator::DEFAULT_ALIGN) override;

    /// Deallocates memory. Will fail if called while the chunk is not allocated.
    virtual void deallocate(void *p) override;

    /// Returns the total amount of memory allocated (not just the total number of currently allocated nodes).
    virtual AddrUint total_allocated() override;

    /// Always returns `_node_size`. Doesn't check if `p` is valid.
    virtual AddrUint allocated_size(void *p) override;

    virtual void *
    reallocate(void *old_allocation, AddrUint new_size, AddrUint align, AddrUint optional_old_size) override;

  private:
    AddrUint _total_allocated;
    void *_mem;
};

} // namespace fo
