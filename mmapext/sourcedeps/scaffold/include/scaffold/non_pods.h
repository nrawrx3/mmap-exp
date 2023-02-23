#pragma once

#include <scaffold/memory.h>
#include <scaffold/rbt.h>
#include <scaffold/types.h>

#include <initializer_list>

namespace fo {

/// Pretty much like std::vector, but doesn't value-initialize elements on a resize() if element type is
/// trivially default constructible. Provides a `resize_with_given` function instead. Use some caution
/// therefore when using it since it's not a drop-in replacement for std::vector usually.
template <typename T> struct Vector {
    T *_data;
    u32 _size;
    u32 _capacity;
    fo::Allocator *_allocator;

    using iterator = T *;
    using const_iterator = const T *;

    Vector(u32 initial_count = 0, fo::Allocator &a = fo::memory_globals::default_allocator());

    Vector(u32 initial_count,
           const T &fill_element,
           fo::Allocator &a = fo::memory_globals::default_allocator());

    Vector(fo::Allocator &a);

    Vector(std::initializer_list<T> init_list);

    Vector(const Vector &);
    Vector(Vector &&);

    ~Vector();

    Vector &operator=(const Vector &);
    Vector &operator=(Vector &&);

    T &operator[](i32 n) { return _data[n]; }
    const T &operator[](i32 n) const { return _data[n]; }

    T *begin() { return _data; }
    T *end() { return _data + _size; }
    const T *begin() const { return _data; }
    const T *end() const { return _data + _size; }

    const T *cbegin() { return _data; }
    const T *cend() { return _data + _size; }

    const T *cbegin() const { return _data; }
    const T *cend() const { return _data + _size; }
};

template <typename Key, typename Value> struct OrderedMap {
    using Rbt = fo::rbt::RBTree<Key, Value>;

    Rbt _rbt;

    using iterator = fo::rbt::Iterator<Key, Value, false>;
    using const_iterator = fo::rbt::Iterator<Key, Value, true>;

    OrderedMap(fo::Allocator &allocator = fo::memory_globals::default_allocator(),
               typename Rbt::is_less_fn less_fn = std::less<Key>{})
        : _rbt(allocator, less_fn) {}

    ~OrderedMap() {}

    // Take an rbt and use as an OrderedMap
    OrderedMap(const Rbt &rbt)
        : _rbt(rbt) {}

    // Same as above.
    OrderedMap(Rbt &&rbt)
        : _rbt(std::move(rbt)) {}

    OrderedMap(const OrderedMap &) = default;
    OrderedMap &operator=(OrderedMap &&) = default;
    OrderedMap &operator=(const OrderedMap &) = default;
    OrderedMap(OrderedMap &&) = default;

    inline Value &operator[](const Key &k);
    inline const Value &operator[](const Key &k) const;

    iterator begin() { return rbt::begin(_rbt); }
    iterator end() { return rbt::end(_rbt); }

    const_iterator begin() const { return rbt::begin(_rbt); }
    const_iterator end() const { return rbt::end(_rbt); }

    const_iterator cbegin() { return rbt::begin(static_cast<const Rbt &>(_rbt)); }
    const_iterator cend() { return rbt::end(static_cast<const Rbt &>(_rbt)); }

    const_iterator cbegin() const { return rbt::begin(_rbt); }
    const_iterator cend() const { return rbt::end(_rbt); }
};

} // namespace fo
