#include <scaffold/buddy_allocator.h>
#include <scaffold/timed_block.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace fo {

namespace buddy_allocator_internal {

struct BuddyHead {
    // I could get rid of the prev pointer. It sure makes removing a buddy easier, and it won't usually be the
    // case that leaf-buddies will be smaller that 16 bytes.
    BuddyHead *_next;
    BuddyHead *_prev;

    BuddyHead()
        : _next(nullptr)
        , _prev(nullptr) {}

    BuddyHead(BuddyHead *next, BuddyHead *prev)
        : _next(next)
        , _prev(prev) {}

    /// Removes this header from the free list given
    void remove_self_from_list(BuddyHead **free_lists, int level) {
        if (_prev) {
            _prev->_next = _next;
            if (_next) {
                _next->_prev = _prev;
            }
        } else {
            // Need to set the free list to _next if it was the head of the list
            free_lists[level] = _next;
            if (_next) {
                _next->_prev = nullptr;
            }
        }
        make_meaningless();
    }

    /// Makes the header meaninglesss in debug configuration. This is for checking correctness and nothing
    /// else.
    void make_meaningless() {
#ifndef NDEBUG
        _next = nullptr;
        _prev = nullptr;
#endif
    }

    /// If in debug configuration, checks if buddy is meaningless
    bool is_meaningless() {
#ifndef NDEBUG
        return _next == nullptr && _prev == nullptr;
#else
        return true;
#endif
    }
};
} // namespace buddy_allocator_internal

using buddy_allocator_internal::BuddyHead;

static inline bool alignment_ok(AddrUint requested_align) {
    if (requested_align < alignof(BuddyHead)) {
        return alignof(BuddyHead) % requested_align == 0;
    } else {
        return requested_align % alignof(BuddyHead) == 0;
    }
}

inline AddrUint BuddyAllocator::_buddy_size_at_level(AddrUint level) const { return _buffer_size >> level; }

BuddyAllocator::BuddyAllocator(AddrUint size,
                               AddrUint min_buddy_size,
                               bool abort_on_allocation_failure,
                               Allocator &main_allocator,
                               Allocator &extra_allocator,
                               const char *allocator_name)
    : _buffer_size{ clip_to_pow2(size) }
    , _leaf_buddy_size{ min_buddy_size }
    , _num_levels{ log2_ceil(_buffer_size / _leaf_buddy_size) + 1 }
    , _leaf_buddy_size_power{ AddrUint(log2_ceil(_leaf_buddy_size)) }
    , _num_indices{ AddrUint(1) << _last_level() }
    , _free_lists{ nullptr }
    , _leaf_allocated{ 1, unsigned(_num_indices), extra_allocator }
    , _level_of_leaf{ unsigned(log2_ceil(_num_levels)), unsigned(_num_indices), extra_allocator }
    , _mem{ nullptr }
    , _main_allocator{ &main_allocator }
    , _extra_allocator{ &extra_allocator }
    , _total_allocated{ 0 }
    , _abort_on_allocation_failure(abort_on_allocation_failure) {

    set_name(allocator_name, strlen(allocator_name));

    // log_assert(clip_to_pow2(size) == size, "size given %lu is not a power of 2", size);

    // Allocate the buffer
    _mem = (char *)_main_allocator->allocate(size, alignof(BuddyHead));
    log_assert(_mem != nullptr, "Failed to allocated buffer");

    // Create the free list array
    const auto array_size = sizeof(BuddyHead *) * _num_levels;
    _free_lists = (BuddyHead **)extra_allocator.allocate(array_size, alignof(BuddyHead *));
    memset(_free_lists, 0, array_size);

    AddrUint free_list_array_size = array_size * sizeof(BuddyHead *);
    AddrUint leaf_allocated_size = _num_indices / 8;
    AddrUint level_map_size = log2_ceil(_num_levels) * _num_indices / 8;
    AddrUint extra_overhead = free_list_array_size + leaf_allocated_size + level_map_size;

    // Now, there's could be a portion to the left of the buffer which we must make unavailable by marking it
    // as allocated. This is the size of that portion. The rest of this code deals with that.
    _unavailable = clip_to_pow2(_buffer_size - size);
    _mem = _mem - _unavailable;

    // If size was a power of of two, all buddies are free, otherwise, we have to artifically mark the dummy
    // buddy of size `_unavailable` at toward the left
    if (_unavailable == 0) {
        ((BuddyHead *)_mem)->make_meaningless();
        _push_free((BuddyHead *)_mem, 0);
        _free_lists[0] = (BuddyHead *)_mem;
    } else {
        _mark_unavailable_buddy();
    }

    log_info(R"(
        Initialized a BuddyAllocator "%s" with following attributes
            ._buffer_size = %lu (%lu MB)
            ._leaf_buddy_size = %lu (%lu KB)
            ._leaf_buddy_size_power = %lu
            ._num_levels = %lu
            .starting size = %lu
            ._unavailable = %lu
            ._num_indices = %lu
            extra_overhead = %lu bytes
        )",

             name(),
             CAST_TO_LU(_buffer_size),
             CAST_TO_LU(_buffer_size >> 20),
             CAST_TO_LU(_leaf_buddy_size),
             CAST_TO_LU(_leaf_buddy_size >> 10),
             CAST_TO_LU(_leaf_buddy_size_power),
             CAST_TO_LU(_num_levels),
             CAST_TO_LU(_buffer_size - _unavailable),
             CAST_TO_LU(_unavailable),
             CAST_TO_LU(1lu << _last_level()),
             CAST_TO_LU(extra_overhead));
}

inline void BuddyAllocator::_mark_unavailable_buddy() {
    // Level at which buddy size is equal to unavailable
    uint32_t l = log2_ceil(_buffer_size / _unavailable);

    assert(_buddy_size_at_level(l) == _unavailable);

    // Keep breaking levels upto, but excluding, the level with buddy size
    // equal to the unavailable size.
    for (uint32_t i = 0; i < l; ++i) {
        AddrUint num_leaves = _leaves_contained(i + 1);
        _level_of_leaf.set_range(num_leaves, num_leaves + num_leaves, i + 1);

        // The free list for this level also needs to be set up
        BuddyHead *b = (BuddyHead *)(_mem + _buddy_size_at_level(i + 1));
        b->_next = b->_prev = nullptr;
        _free_lists[i + 1] = b;
    }
    // Mark the first buddy of level l as allocated and set up the free list.
    _leaf_allocated.set_range(0, _leaves_contained(l), 1);

    _dbg_print_levels(0, _leaves_contained(0));
}

BuddyAllocator::~BuddyAllocator() {
    _main_allocator->deallocate(_mem + _unavailable);
    if (_total_allocated != 0) {
        log_err("BuddyAllocator::Leaking memory?! - Total Allocated = " ADDRUINT_FMT, _total_allocated);
        assert(0);
    }
    _extra_allocator->deallocate(_free_lists);
}

AddrUint BuddyAllocator::total_allocated() { return _total_allocated; }

AddrUint BuddyAllocator::allocated_size(void *p) {
    const AddrUint idx = _leaf_index((BuddyHead *)p);
    const AddrUint level = _level_of_leaf.get(idx);
    return _buddy_size_at_level(level);
}

void *BuddyAllocator::allocate(AddrUint size, AddrUint align) {
    TIMED_BLOCK;

    (void)align; // unused
    size = clip_to_pow2(size);

    log_assert(size >= _leaf_buddy_size,
               "Cannot allocate a buddy size smaller than the minimum size of " ADDRUINT_FMT,
               _leaf_buddy_size);

    log_assert(alignment_ok(align), "Alignment of " ADDRUINT_FMT " is not valid", align);

    int64_t level = _last_level();
    debug("Allocating buddy of size " ADDRUINT_FMT " bytes", size);
    while (true) {
        TIMED_BLOCK;

        const AddrUint buddy_size = _buddy_size_at_level(AddrUint(level));

        // Either buddies are not big enough  or none are free, either
        // way, need to go to upper level
        if (buddy_size < size || _free_lists[level] == nullptr) {
            --level;

            if (level < 0) {
                log_err("%s - Failed to allocate %lu bytes, aborting...? %s",
                        __PRETTY_FUNCTION__,
                        CAST_TO_LU(size),
                        _abort_on_allocation_failure ? "Yes" : "No");
                if (_abort_on_allocation_failure) {
                    abort();
                }
                return nullptr;
            }

            continue;
        }

        // OK, at least 1 free and buddy size is actually a greater power of 2, break it in half and put the
        // buddies in the lower level just below, and check the lower level
        if (buddy_size > size) {
            const AddrUint index = _leaf_index(_free_lists[level]);
            _dbg_print_levels(index, index + _leaves_contained(level));
            debug("%s - i:%lu - level - %li", __PRETTY_FUNCTION__, CAST_TO_LU(index), CAST_TO_LU(level));
            _break_free(level);
            ++level;
            _dbg_print_levels(0, _num_indices);
        } else if (buddy_size == size) {
            // Got the buddy with the exact size
            BuddyHead *h = _free_lists[level];
            const AddrUint index = _leaf_index(h);

            log_assert(_level_of_leaf.get(index) == (AddrUint)level,
                       "%s - Bad index - %lu, Freelist level = %li, Stored level = %lu",
                       __PRETTY_FUNCTION__,
                       CAST_TO_LU(index),
                       long(level),
                       CAST_TO_LU(_level_of_leaf.get(index)));

            const AddrUint next_index = _leaves_contained(level);

            debug("%s - Found exact size(%lu buddies) to allocate - i:%lu",
                  __PRETTY_FUNCTION__,
                  CAST_TO_LU(next_index),
                  CAST_TO_LU(index));

            _dbg_print_levels(index, next_index >= _num_indices ? _num_indices : next_index);
            h->remove_self_from_list(_free_lists, level);
            // Set each to allocated status
            {
                TIMED_BLOCK;
                _leaf_allocated.set_range(index, index + _leaves_contained(level), 1);
            }

            _total_allocated += size;
            debug("%s - Allocated buddy. Level - %li, i:%lu (Size = %lu)\n--",
                  __PRETTY_FUNCTION__,
                  long(level),
                  CAST_TO_LU(index),
                  CAST_TO_LU(size));
            _dbg_print_levels(index, next_index);
            return (void *)h;
        }
    }

    assert(false && "Unreachable");

    return nullptr; // Unreachable
}

void BuddyAllocator::deallocate(void *p) {
    TIMED_BLOCK;

    if (p == nullptr)
        return;

    // Corresponding header pointer - make it meaningless for correctness.
    BuddyHead *h = _head_at(p);
    h->make_meaningless();

    _check_leaf_index(h);

    // Get the index of the buddy as known from the address and then the
    // level and size of this buddy
    AddrUint idx = _leaf_index(h);
    const AddrUint original_level = _level_of_leaf.get(idx);
    AddrUint level = original_level;

    assert(_leaf_allocated.get(idx));

    const AddrUint size = _buddy_size_at_level((AddrUint)level);

    if (!((int64_t)_total_allocated - (int64_t)size >= 0)) {
        log_assert(
            false,
            R"(%s -- Should not happen index (index_allocated = %d?) i:%lu, level - %lu, size - %lu, _total_allocated - %lu)",
            __PRETTY_FUNCTION__,
            int(_leaf_allocated.get(idx)),
            CAST_TO_LU(idx),
            CAST_TO_LU(level),
            CAST_TO_LU(size),
            CAST_TO_LU(_total_allocated));
        abort();
    }

    _dbg_print_levels(idx, idx + 20);

    _leaf_allocated.set(idx, 0);
    _total_allocated -= size;

    // Put h back into free list
    h->make_meaningless();
    _push_free(h, level);

    // Try to keep merging
    BuddyHead *left = h;
    BuddyHead *right = nullptr;
    BuddyHead *tmp = nullptr;

    debug("%s - set initial block headed by i:%lu to free", __PRETTY_FUNCTION__, CAST_TO_LU(idx));
    _dbg_print_levels(idx, idx + 20 >= _num_indices ? _num_indices : idx + 20);

    while (level >= 1) {
        TIMED_BLOCK;

        debug("%s - MERGE START - %lu, level - %lu",
              __PRETTY_FUNCTION__,
              CAST_TO_LU(original_level - level),
              CAST_TO_LU(level));

        _dbg_print_levels(0, _num_indices);

        const AddrUint size = _buddy_size_at_level(level);
        const AddrUint buddies_inside = _leaves_contained(level);
        AddrUint left_idx = _leaf_index(left);
        AddrUint right_idx = left_idx + buddies_inside;

        // Suppose that a buddy head is pointing to a buddy currently located at level 'n'. The level can be
        // 'chunked' into _leaves_contained(n) buddies of size _buddy_size_at_level(n). We want to know the
        // index of the buddy our current buddy head is pointing at level n, if we see level n as such an
        // array. If that index is odd, the buddy we should merge with is located to the left, otherwise it's
        // to the right.
        const AddrUint level_idx = left_idx / _leaves_contained(level);
        if (level_idx % 2 != 0) {
            tmp = left;
            left = (BuddyHead *)((char *)left - size);
            right = tmp;
            left_idx = left_idx - buddies_inside;
            right_idx = left_idx + buddies_inside;
        } else {
            right = (BuddyHead *)((char *)left + size);
        }

        // One of the buddies left or right is guaranteed to be free, but we don't track which to keep it all
        // simple. Same thing for level. One of them is guaranteed to be in the current `level`.
        if (!_leaf_allocated.get(left_idx) && !_leaf_allocated.get(right_idx) &&
            _level_of_leaf.get(left_idx) == _level_of_leaf.get(right_idx)) {
            debug("\tBEFORE MERGE - left i:%lu and right i:%lu", CAST_TO_LU(left_idx), CAST_TO_LU(right_idx));

            // Print the surrounding buddy status
            _dbg_print_levels(idx, idx + 20 >= _num_indices ? _num_indices : idx + 20);

            left->remove_self_from_list(_free_lists, level);
            right->remove_self_from_list(_free_lists, level);
            --level;
            _push_free(left, level);

            debug("BuddyAlloc::AFTER MERGE - left i:%lu and right i:%lu",
                  CAST_TO_LU(left_idx),
                  CAST_TO_LU(right_idx));
            _dbg_print_levels(idx, idx + 20 >= _num_indices ? _num_indices : idx + 20);
        } else {
            debug("BuddyAlloc::DONE MERGING");
            break;
        }
    }

    debug(R"(%s - Finish
        Prev level = %lu, Cur level = %lu i:%lu, Size_diff = -%lu)
        --
    )",
          __PRETTY_FUNCTION__,
          CAST_TO_LU(original_level),
          CAST_TO_LU(level),
          CAST_TO_LU(idx),
          CAST_TO_LU(size));
}

inline buddy_allocator_internal::BuddyHead *BuddyAllocator::_head_at(void *p) {
#ifndef NDEBUG
    assert(p >= _mem);
    AddrUint diff = (char *)p - _mem;
    int mod = diff % _leaf_buddy_size;
    assert(mod == 0);
#endif
    return static_cast<buddy_allocator_internal::BuddyHead *>(p);
}

inline AddrUint BuddyAllocator::_leaf_index(buddy_allocator_internal::BuddyHead *p) const {
    assert((char *)p >= _mem);
    assert((char *)p <= _mem + _buffer_size);
    const AddrUint diff = (char *)p - _mem;
    // debug("diff = %lu", diff);
    return AddrUint(diff >> _leaf_buddy_size_power);
}

inline AddrUint BuddyAllocator::_leaves_contained(AddrUint level) const {
    return AddrUint(1) << (_last_level() - level);
}

BuddyHead *BuddyAllocator::_break_free(AddrUint level) {
    TIMED_BLOCK;

    const AddrUint new_level = level + 1;

    BuddyHead *h_level = _free_lists[level];
    BuddyHead *h1 = h_level;
    BuddyHead *h2 =
        reinterpret_cast<BuddyHead *>(reinterpret_cast<char *>(h_level) + _buddy_size_at_level(new_level));

#if defined(BUDDY_ALLOC_LEVEL_CHECKS) && defined(NDEBUG)
    if (!(_level_of_leaf.get(_leaf_index(h1)) == level)) {
        log_err("%s - h1's noted level(index = %lu) = %lu, but found in level %lu",
                __PRETTY_FUNCTION__,
                _leaf_index(h1),
                _level_of_leaf.get(_leaf_index(h1)),
                level);
        abort();
    }
    if (!(_level_of_leaf.get(_leaf_index(h2)) == level)) {
        log_err("%s - h2's(index = %lu) level = %lu, but found in level %lu",
                __PRETTY_FUNCTION__,
                _leaf_index(h2),
                _level_of_leaf.get(_leaf_index(h2)),
                level);
        abort();
    }
#endif

    h_level->remove_self_from_list(_free_lists, level);

    // `_push_free` will assert this
    h1->make_meaningless();
    h2->make_meaningless();
    _push_free(h2, new_level);
    _push_free(h1, new_level);

    return h1;
}

void BuddyAllocator::_push_free(BuddyHead *h, AddrUint level) {
    TIMED_BLOCK;

    assert(h->is_meaningless() && "Must be meaningless");
    assert(h != _free_lists[level] && "This really should not happen");

    h->_next = _free_lists[level];
    h->_prev = nullptr;
    if (_free_lists[level] != nullptr) {
        _free_lists[level]->_prev = h;
    }
    _free_lists[level] = h;
    const AddrUint index = _leaf_index(h);
    const AddrUint last = index + _leaves_contained(level);

    debug("BuddyAlloc::Setting levels - h = %p, level = %lu, index = %lu, last = %lu",
          h,
          CAST_TO_LU(level),
          CAST_TO_LU(index),
          CAST_TO_LU(last));

    _level_of_leaf.set_range(index, last, level);
    _leaf_allocated.set_range(index, last, 0);

    debug("Pushed free, done setting %lu to %lu to level %lu",
          CAST_TO_LU(index),
          CAST_TO_LU(last),
          CAST_TO_LU(level));
    /*
    for (AddrUint b = index; b < last; ++b) {
        _leaf_allocated.set(b, 0);
    }
    */
}

void BuddyAllocator::_check_leaf_index(BuddyHead *p) const {
#ifndef NDEBUG
    AddrUint index = _leaf_index(p);
    AddrUint level = _level_of_leaf.get(index);
    AddrUint buddies_inside = _leaves_contained(level);

    AddrUint mod = index % buddies_inside;
    if (mod != 0) {
        log_err("Buddy block of size %lu can never begin at index %lu and be at level %lu",
                CAST_TO_LU(buddies_inside << _leaf_buddy_size_power),
                CAST_TO_LU(index),
                CAST_TO_LU(level));
        abort();
    }
#else
    (void)p;
#endif
}

void BuddyAllocator::_dbg_print_levels(AddrUint start, AddrUint end) const {
#ifdef BUDDY_ALLOC_LEVEL_LOGGING
    using namespace string_stream;
    Buffer b(memory_globals::default_allocator());
    fprintf(stderr, "+--------LEVEL MAP--------+\n");
    for (int i = int(start); i < int(end); ++i) {
        b << i << "= (" << _level_of_leaf.get(i) << ", " << (_leaf_allocated.get(i) ? "x" : "o") << ")\t";
        tab(b, 8);
        if (size(b) >= 80) {
            fprintf(stderr, "%s\n", c_str(b));
            clear(b);
        }
    }
    if (size(b) != 0) {
        fprintf(stderr, "%s\n", c_str(b));
        clear(b);
    }
    fprintf(stderr, "\n--\n");

    for (AddrUint i = 0; i < _num_levels; ++i) {
        BuddyHead *h = _free_lists[i];
        fprintf(stderr, "Free indices at level %lu: ", i);
        while (h) {
            fprintf(stderr, "%lu --> ", _leaf_index(h));
            h = h->_next;
        }
        fprintf(stderr, "NULL\n");
    }
    fprintf(stderr, "+-------END-------+\n**\n");
#else
    (void)start;
    (void)end;
#endif
}
} // namespace fo
