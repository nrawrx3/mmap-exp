#pragma once

#include <scaffold/collection_types.h>
#include <scaffold/const_log.h>
#include <scaffold/memory.h>

#include <limits>
#include <memory>
#include <string.h>

#include <stdio.h>

/// Functions operating on fo::Array

namespace fo {

/// The number of elements in the array.
template <typename T> uint32_t size(const Array<T> &a);
/// Returns true if there are any elements in the array.
template <typename T> bool any(const Array<T> &a);
/// Returns true if the array is empty.
template <typename T> bool empty(const Array<T> &a);

/// Returns the first/last element of the array. Don't use these on an
/// empty array.
template <typename T> T &front(Array<T> &a);
template <typename T> const T &front(const Array<T> &a);
template <typename T> T &back(Array<T> &a);
template <typename T> const T &back(const Array<T> &a);

/// Returns pointer to first element of array
template <typename T> T *data(Array<T> &a);
template <typename T> const T *data(const Array<T> &a);

/// Changes the size of the array (does not reallocate memory unless necessary).
template <typename T> void resize(Array<T> &a, uint32_t new_size);
/// Removes all items in the array but does not free memory
template <typename T> void clear(Array<T> &a);
/// Removes all items in the array and frees memory
template <typename T> void free(Array<T> &a);
/// Reallocates the array to the specified capacity.
template <typename T> void set_capacity(Array<T> &a, uint32_t new_capacity);
/// Makes sure that the array has at least the specified capacity. (If not,
/// the array is grown.)
template <typename T> void reserve(Array<T> &a, uint32_t new_capacity);
/// Grows the array using a geometric progression formula, so that the
/// ammortized cost of push_back() is O(1). If a min_capacity is specified,
/// the array will grow to at least that capacity.
template <typename T> void grow(Array<T> &a, uint32_t min_capacity = 0);
/// Trims the array so that its capacity matches its size.
template <typename T> void trim(Array<T> &a);

/// Pushes the item to the end of the array.
template <typename T> void push_back(Array<T> &a, const T &item);
/// Pops the last item from the array. The array cannot be empty.
template <typename T> void pop_back(Array<T> &a);

// -- Treat an array like a set of items. operator== must be defined for the element type T. All operations
// are O(n), so it's good only for small sets.

/// Add item to set. Return the index of the item in the array. Does not invalidate older indices.
template <typename T> u32 add_to_set(Array<T> &a, const T &item);

/// Returns index to item if the given item is present in set. Returns ~u32(0) otherwise.
template <typename T> u32 exists_in_set(const Array<T> &a, const T &item);

/// Removes item from set if it exists. *Invalidates older indices*.
template <typename T> u32 remove_from_set(Array<T> &a, const T &item);

template <typename T> inline uint32_t size(const Array<T> &a) { return a._size; }
template <typename T> inline bool any(const Array<T> &a) { return a._size != 0; }
template <typename T> inline bool empty(const Array<T> &a) { return a._size == 0; }

/// Keeping these two `begin` and `end` functions in this namespace to be
/// compatible with the original fo code.
template <typename T> typename Array<T>::iterator begin(Array<T> &a) { return a.begin(); }
template <typename T> inline typename Array<T>::iterator end(Array<T> &a) { return a.end(); }

template <typename T> typename Array<T>::const_iterator begin(const Array<T> &a) { return a.begin(); }
template <typename T> inline typename Array<T>::const_iterator end(const Array<T> &a) { return a.end(); }

template <typename T> inline T &front(Array<T> &a) { return a._data[0]; }
template <typename T> inline const T &front(const Array<T> &a) { return a._data[0]; }
template <typename T> inline T &back(Array<T> &a) { return a._data[a._size - 1]; }
template <typename T> inline const T &back(const Array<T> &a) { return a._data[a._size - 1]; }

template <typename T> T *data(Array<T> &a) { return (T *)a._data; }
template <typename T> const T *data(const Array<T> &a) { return (const T *)a._data; }

template <typename T> inline void clear(Array<T> &a) { resize(a, 0); }

template <typename T> inline void free(Array<T> &a) {
    a._allocator->deallocate(a._data);
    a._data = nullptr;
    a._size = 0;
}
template <typename T> inline void trim(Array<T> &a) { set_capacity(a, a._size); }

template <typename T> void resize(Array<T> &a, uint32_t new_size) {
    if (new_size > a._capacity) {
        grow(a, new_size);
// Zero the memory if we are in debug mode
#ifndef NDEBUG
        if (std::is_trivial<T>::value) {
            memset(&a._data[a._size], 0, (new_size - a._size) * sizeof(T));
        } else {
            std::fill(&a._data[a._size], a._data + new_size, T{});
        }
#endif
    }
    a._size = new_size;
}

template <typename T> inline void reserve(Array<T> &a, uint32_t new_capacity) {
    if (new_capacity > a._capacity)
        set_capacity(a, new_capacity);
}

#if 0
template <typename T> void set_capacity(Array<T> &a, uint32_t new_capacity) {
    if (new_capacity == a._capacity)
        return;

    if (new_capacity < a._size)
        resize(a, new_capacity);

    T *new_data = 0;
    if (new_capacity > 0) {
        new_data = (T *)a._allocator->allocate(sizeof(T) * new_capacity, alignof(T));
        memcpy(new_data, a._data, sizeof(T) * a._size);
    }
    a._allocator->deallocate(a._data);
    a._data = new_data;
    a._capacity = new_capacity;
}

#endif

template <typename T> void set_capacity(Array<T> &a, uint32_t new_capacity) {
    if (new_capacity == a._capacity) {
        return;
    }

    if (new_capacity < a._size) {
        resize(a, new_capacity);
    }

    T *new_data = nullptr;
    if (new_capacity > 0) {
        new_data = (T *)a._allocator->reallocate(
            a._data, sizeof(T) * new_capacity, alignof(T), sizeof(T) * a._capacity);
    }
    a._data = new_data;
    a._capacity = new_capacity;
}

template <typename T> void grow(Array<T> &a, uint32_t min_capacity) {
    uint32_t new_capacity = a._capacity ? a._capacity * 2 : 2;
    if (new_capacity < min_capacity) {
        new_capacity = min_capacity;
        new_capacity = clip_to_pow2(new_capacity);
    }
    set_capacity(a, new_capacity);
}

template <typename T> inline void push_back(Array<T> &a, const T &item) {
    if (a._size + 1 > a._capacity)
        grow(a);
    a._data[a._size++] = item;
}

template <typename T> inline void pop_back(Array<T> &a) { a._size--; }

template <typename T> u32 add_to_set(Array<T> &a, const T &item) {
    for (u32 i = 0; i < size(a); ++i) {
        if (a[i] == item) {
            return i;
        }
    }
    push_back(a, item);
    return size(a) - 1;
}

template <typename T> u32 exists_in_set(const Array<T> &a, const T &item) {
    for (u32 i = 0; i < size(a); ++i) {
        if (a[i] == item) {
            return i;
        }
    }
    return std::numeric_limits<u32>::max();
}

template <typename T> u32 remove_from_set(Array<T> &a, const T &item) {
    for (u32 i = 0; i < size(a); ++i) {
        if (a[i] == item) {
            std::swap(a[i], a[size(a) - 1]);
            pop_back(a);
            return i;
        }
    }
    return std::numeric_limits<u32>::max();
}

template <typename T>
inline Array<T>::Array(Allocator &allocator, uint32_t initial_size)
    : _allocator(&allocator)
    , _size(0)
    , _capacity(0)
    , _data(nullptr) {
    resize(*this, initial_size);
}

template <typename T>
Array<T>::Array(std::initializer_list<T> init_list, Allocator &allocator)
    : _allocator(&allocator)
    , _size(0)
    , _capacity(0)
    , _data(nullptr) {
    reserve(*this, init_list.size());

    for (const auto &item : init_list) {
        push_back(*this, item);
    }
}

template <typename T> inline Array<T>::~Array() {
    if (_data) {
        _allocator->deallocate(_data);
        _size = 0;
        _capacity = 0;
        _data = nullptr;
        _allocator = nullptr;
    }
}

template <typename T>
Array<T>::Array(const Array<T> &other)
    : _allocator(other._allocator)
    , _size(0)
    , _capacity(0)
    , _data(0) {
    const uint32_t n = other._size;
    set_capacity(*this, n);
    memcpy(_data, other._data, sizeof(T) * n);
    _size = n;
}

template <typename T>
Array<T>::Array(Array<T> &&other)
    : _allocator(other._allocator)
    , _size(other._size)
    , _capacity(other._capacity)
    , _data(other._data) {
    other._size = 0;
    other._capacity = 0;
    other._data = nullptr;
}

template <typename T> Array<T> &Array<T>::operator=(const Array<T> &other) {
    const uint32_t n = other._size;
    resize(*this, n);
    memcpy(_data, other._data, sizeof(T) * n);
    return *this;
}

template <typename T> Array<T> &Array<T>::operator=(Array<T> &&other) {
    if (this != &other) {
        _allocator->deallocate(_data);
        _data = other._data;
        _size = other._size;
        _capacity = other._capacity;
        _allocator = other._allocator;
        // Set other array to an empty state
        other._size = 0;
        other._capacity = 0;
        other._data = nullptr;
        // However, don't set other._allocator to null.
    }
    return *this;
}

template <typename T> inline T &Array<T>::operator[](uint32_t i) { return _data[i]; }

template <typename T> inline const T &Array<T>::operator[](uint32_t i) const { return _data[i]; }

} // namespace fo
