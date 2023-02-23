#pragma once

#include <scaffold/const_log.h>
#include <scaffold/debug.h>
#include <scaffold/non_pods.h>

#include <algorithm>

namespace fo {

template <typename T> void resize(Vector<T> &a, u32 new_size);
template <typename T> void resize_with_given(Vector<T> &a, u32 new_size, const T &t);
template <typename T> u32 reserve(Vector<T> &a, u32 new_capacity);
template <typename T> u32 size(const Vector<T> &a);
template <typename T> u32 capacity(const Vector<T> &a);

template <typename T> T &push_back(Vector<T> &a, const T &element);
template <typename T> T &push_back(Vector<T> &a, T &&element);
template <typename T, typename... CtorArgs> T &emplace_back(Vector<T> &a, CtorArgs &&... ctor_args);
template <typename T> void pop_back(Vector<T> &a);
template <typename T> void clear(Vector<T> &a);

template <typename T> const T &back(const Vector<T> &a);
template <typename T> const T &front(const Vector<T> &a);
template <typename T> T &back(Vector<T> &a);
template <typename T> T &front(Vector<T> &a);

template <typename T> T *data(Vector<T> &a);
template <typename T> const T *data(const Vector<T> &a);

template <typename T> T *begin(Vector<T> &a) { return data(a); }
template <typename T> T *end(Vector<T> &a) { return data(a) + a._size; }
template <typename T> const T *begin(const Vector<T> &a) { return data(a); }
template <typename T> const T *end(const Vector<T> &a) { return data(a) + a._size; }

template <typename T>
void resize_and_set(Vector<T> &a, u32 i, const T &element, const T &default_element = T{});

namespace internal {

template <typename T> void grow(Vector<T> &a);
template <typename T> void move(T *source, T *destination, u32 source_size);
template <typename T> void copy(T *source, T *destination, u32 source_size);
template <typename T> void destroy_elements(T *elements, u32 count);
template <typename T> void fill_with_default(T *elements, u32 count);
template <typename T> void fill_with_given(T *elements, u32 count, const T &element);

// With C++17 we can simply use an if-constexpr
#if 0
template <typename T, bool is_resize> void set_capacity(Vector<T> &a, u32 new_capacity) {
    if (new_capacity == a._capacity) {
        return;
    }

    T *new_allocation = reinterpret_cast<T *>(
        a._allocator->allocate(new_capacity * sizeof(T), std::max(alignof(T), size_t(16))));

    log_assert(new_allocation != nullptr, "Failed to allocate capacity: %u", new_capacity);

    move(a._data, new_allocation, a._size);

    // If storing non-pod-ish elements and resizing, store default constructed elements.
    if constexpr (is_resize && !std::is_trivially_constructible<T>::value) {
        for (u32 i = a._size; i < new_capacity; ++i) {
            new (new_allocation + i) T{};
        }
    }

    a._allocator->deallocate(a._data);
    a._data = new_allocation;
    a._capacity = new_capacity;
}

#endif

// But since we want to be cool with C++14 too, we go through some chore.

template <typename T, bool is_resize> struct FillWithDefaultIfResize;

template <typename T> struct FillWithDefaultIfResize<T, true> {
    static void call(T *mem, T *end) {
        for (T *p = mem; p < end; ++p) {
            new ((void *)p) T();
        }
    }
};

template <typename T> struct FillWithDefaultIfResize<T, false> {
    static void call(T *mem, T *end) {
        (void)mem;
        (void)end;
    }
};

template <typename T,
          bool is_resize,
          std::enable_if_t<!std::is_trivially_default_constructible<T>::value> *whatever = nullptr>
void init_with_defaults_if_nonpod(T *mem, T *end) {
    FillWithDefaultIfResize<T, is_resize>::call(mem, end);
}

template <typename T,
          bool is_resize,
          std::enable_if_t<std::is_trivially_default_constructible<T>::value> *whatever = nullptr>
void init_with_defaults_if_nonpod(T *mem, T *end) {
    (void)mem;
    (void)end;
}

template <typename T, bool is_resize> void set_capacity(Vector<T> &a, u32 new_capacity) {
    if (new_capacity == a._capacity) {
        return;
    }

    T *new_allocation = reinterpret_cast<T *>(
        a._allocator->allocate(new_capacity * sizeof(T), std::max(alignof(T), size_t(16))));

    log_assert(new_allocation != nullptr, "Failed to allocate capacity: %u", new_capacity);

    move(a._data, new_allocation, a._size);

    init_with_defaults_if_nonpod<T, is_resize>(new_allocation + a._size, new_allocation + new_capacity);

    a._allocator->deallocate(a._data);
    a._data = new_allocation;
    a._capacity = new_capacity;
}

}; // namespace internal

} // namespace fo

// ## Implementation

namespace fo {

template <typename T> u32 reserve(Vector<T> &a, u32 new_capacity) {
    if (new_capacity < a._capacity) {
        return a._capacity;
    }
    internal::set_capacity<T, false>(a, u32(1) << log2_ceil(new_capacity));
    return a._capacity;
}

template <typename T> void resize(Vector<T> &a, u32 new_size) {
    static_assert(std::is_default_constructible<T>::value,
                  "Stored element type is not default-ctorable. Use resize_with_given instead");

    if (new_size <= a._size) {
        // ^ This case handles new_size == 0 case too.
        internal::destroy_elements(&a._data[new_size], a._size - new_size);
        a._size = new_size;
        return;
    }

    if (a._size < new_size && new_size < a._capacity) {
        internal::fill_with_default(&a._data[a._size], new_size - a._size);
        a._size = new_size;
        return;
    }

    // Means a._capacity <= new_size.
    u32 new_capacity = u32(1) << log2_ceil(new_size);
    internal::set_capacity<T, true>(a, new_capacity);
    internal::fill_with_default(&a._data[a._size], new_size - a._size);
    a._size = new_size;
}

template <typename T> void resize_with_given(Vector<T> &a, u32 new_size, const T &t) {
    if (new_size < a._size) {
        internal::destroy_elements(&a._data[new_size], a._size - new_size);
        a._size = new_size;
        return;
    }

    if (a._size < new_size && new_size < a._capacity) {
        internal::fill_with_given(&a._data[a._size], new_size - a._size, t);
        return;
    }

    // Means a._capacity <= new_size
    u32 new_capacity = u32(1) << log2_ceil(new_size);
    internal::set_capacity<T, false>(a, new_capacity);

    // Copy-construct each new element
    for (u32 i = a._size; i < new_size; ++i) {
        new (&a._data[i]) T(t);
    }

    a._size = new_size;
}

template <typename T> void resize_and_set(Vector<T> &a, u32 i, const T &element, const T &default_element) {
    if (size(a) <= i) {
        fo::resize_with_given(a, i + 1, default_element);
    }

    a[i] = element;
}

#if 0
template <typename T, typename E> T &push_back(Vector<T> &a, const E &element) {
    if (a._size == a._capacity) {
        internal::grow(a);
    }

    if
        SCAFFOLD_IF_CONSTEXPR(std::is_trivially_copy_constructible<T>::value) {
            a._data[a._size++] = element;
        }
    else {
        new (a._data + (a._size++)) T(element);
    }
    return a._data[a._size - 1];
}

#endif

template <typename T> T &push_back(Vector<T> &a, T &&element) {
    if (a._size == a._capacity) {
        internal::grow(a);
    }

    if
        SCAFFOLD_IF_CONSTEXPR(std::is_trivially_move_constructible<T>::value) {
            a._data[a._size++] = element;
        }
    else {
        new (a._data + (a._size++)) T(std::forward<T>(element));
    }

    return a._data[a._size - 1];
}

template <typename T> T &push_back(Vector<T> &a, const T &element) {
    if (a._size == a._capacity) {
        internal::grow(a);
    }

    if
        SCAFFOLD_IF_CONSTEXPR(std::is_trivially_move_constructible<T>::value) {
            a._data[a._size++] = element;
        }
    else {
        new (a._data + (a._size++)) T(element);
    }

    return a._data[a._size - 1];
}

template <typename T, typename... CtorArgs> T &emplace_back(Vector<T> &a, CtorArgs &&... args) {
    static_assert(std::is_constructible<T, decltype(std::forward<CtorArgs>(args))...>::value,
                  "Element type not constructible from given arguments");
    if (a._size == a._capacity) {
        internal::grow(a);
    }
    new (&a._data[a._size]) T(std::forward<CtorArgs>(args)...);
    ++a._size;
    return a._data[a._size - 1];
}

template <typename T> void pop_back(Vector<T> &a) {
    internal::destroy_elements(&a._data[a._size - 1], 1);
    --a._size;
}

template <typename T> void clear(Vector<T> &a) {
    while (fo::size(a) != 0) {
        fo::pop_back(a);
    }
}

template <typename T> const T &back(const Vector<T> &a) { return a._data[a._size - 1]; }

template <typename T> const T &front(const Vector<T> &a) { return a._data[0]; }

template <typename T> T &back(Vector<T> &a) { return a._data[a._size - 1]; }

template <typename T> T &front(Vector<T> &a) { return a._data[0]; }

template <typename T> u32 size(const Vector<T> &a) { return a._size; }

template <typename T> u32 capacity(const Vector<T> &a) { return a._capacity; }

template <typename T> T *data(Vector<T> &a) { return a._data; }

template <typename T> const T *data(const Vector<T> &a) { return a._data; }

template <typename T>
Vector<T>::Vector(u32 initial_count, fo::Allocator &a)
    : _data(nullptr)
    , _size(0)
    , _capacity(0)
    , _allocator(&a) {
    if (initial_count > 0) {
        resize(*this, initial_count);
    }
}

template <typename T>
Vector<T>::Vector(u32 initial_count, const T &fill_element, fo::Allocator &a)
    : _data(nullptr)
    , _size(0)
    , _capacity(0)
    , _allocator(&a) {
    if (initial_count > 0) {
        resize_with_given(*this, initial_count, fill_element);
    }
}

template <typename T>
Vector<T>::Vector(std::initializer_list<T> init_list)
    : _data(nullptr)
    , _size(0)
    , _capacity(0)
    , _allocator(&fo::memory_globals::default_allocator()) {
    reserve(*this, init_list.size());
    // log_info("Reserved - %u elements", capacity(*this));
    for (auto &element : init_list) {
        push_back(*this, std::move(element));
    }
}

template <typename T>
Vector<T>::Vector(const Vector<T> &o)
    : _data(nullptr)
    , _size(o._size)
    , _capacity(o._capacity)
    , _allocator(o._allocator) {
    _data =
        reinterpret_cast<T *>(_allocator->allocate(_capacity * sizeof(T), std::max(alignof(T), size_t(16))));
    internal::copy(o._data, _data, _size);
}

template <typename T>
Vector<T>::Vector(Vector<T> &&o)
    : _data(o._data)
    , _size(o._size)
    , _capacity(o._capacity)
    , _allocator(o._allocator) {
    o._data = nullptr;
    o._size = o._capacity = 0;
}

template <typename T>
Vector<T>::Vector(fo::Allocator &a)
    : _data(nullptr)
    , _size(0)
    , _capacity(0)
    , _allocator(&a) {}

template <typename T> Vector<T>::~Vector() {
    if (_data) {
        internal::destroy_elements(_data, _size);
        _allocator->deallocate(_data);
        _data = nullptr;
        _size = _capacity = 0;
    }
}

template <typename T> Vector<T> &Vector<T>::operator=(Vector<T> &&o) {
    if (&o != this) {
        if (_data) {
            internal::destroy_elements(_data, _size);
            _allocator->deallocate(_data);
        }
        _data = o._data;
        _size = o._size;
        _capacity = o._capacity;
        _allocator = o._allocator;

        o._data = nullptr;
        o._size = o._capacity = 0;
    }

    return *this;
}

template <typename T> Vector<T> &Vector<T>::operator=(const Vector<T> &o) {
    if (&o != this) {
        internal::destroy_elements(_data, _size);
        resize(*this, o._size);
        std::copy(o._data, o._data + o._size, _data);
        internal::copy(o._data, _data, o._size);
    }
    return *this;
}

namespace internal {

template <typename T> void grow(Vector<T> &a) {
    u32 new_capacity = a._capacity == 0 ? 1 : a._capacity * 2;
    set_capacity<T, false>(a, new_capacity);
}

template <typename T> void move(T *source, T *destination, u32 source_size) {
    if
        SCAFFOLD_IF_CONSTEXPR(std::is_trivially_move_constructible<T>::value) {
            memcpy(destination, source, source_size * sizeof(T));
        }
    else {
        for (u32 i = 0; i < source_size; ++i) {
            new (&destination[i]) T(std::move(source[i]));
        }
    }
}

template <typename T> void copy(T *source, T *destination, u32 source_size) {
    if
        SCAFFOLD_IF_CONSTEXPR(std::is_trivially_copy_constructible<T>::value) {
            memcpy(destination, source, source_size * sizeof(T));
        }
    else {
        for (u32 i = 0; i < source_size; ++i) {
            new (&destination[i]) T(source[i]);
        }
    }
}

template <typename T> void destroy_elements(T *elements, u32 count) {
    if
        SCAFFOLD_IF_CONSTEXPR(!std::is_trivially_destructible<T>::value) {
            T *const end = elements + count;
            while (elements != end) {
                elements->~T();
                ++elements;
            }
        }
}

template <typename T> void fill_with_default(T *elements, u32 count) {
    if
        SCAFFOLD_IF_CONSTEXPR(std::is_trivially_default_constructible<T>::value) { return; }
    else {
        T *end = elements + count;
        while (elements != end) {
            // Note that C++11 onwards, `new T()` is value initialization for both class and non-class types.
            new (elements) T;
            ++elements;
        }
    }
}

template <typename T> void fill_with_given(T *elements, u32 count, const T &element) {
    std::fill(elements, elements + count, element);
}

} // namespace internal

} // namespace fo
