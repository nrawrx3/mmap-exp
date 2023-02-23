#pragma once

#include <scaffold/array.h>
#include <scaffold/collection_types.h>

namespace fo {
/// Returns the number of items in the queue.
template <typename T> uint32_t size(const Queue<T> &q);
/// Returns the ammount of free space in the queue/ring buffer.
/// This is the number of items we can push before the queue needs to grow.
template <typename T> uint32_t space(const Queue<T> &q);
/// Makes sure the queue has room for at least the specified number of items.
template <typename T> void reserve(Queue<T> &q, uint32_t size);

/// Pushes the item to the end of the queue.
template <typename T> void push_back(Queue<T> &q, const T &item);
/// Pops the last item from the queue. The queue cannot be empty.
template <typename T> void pop_back(Queue<T> &q);
/// Pushes the item to the front of the queue.
template <typename T> void push_front(Queue<T> &q, const T &item);
/// Pops the first item from the queue. The queue cannot be empty.
template <typename T> void pop_front(Queue<T> &q);

/// Consumes n items from the front of the queue.
template <typename T> void consume(Queue<T> &q, uint32_t n);
/// Pushes n items to the back of the queue.
template <typename T> void push(Queue<T> &q, const T *items, uint32_t n);

/// Returns the begin and end of the continuous chunk of elements at
/// the start of the queue. (Note that this chunk does not necessarily
/// contain all the elements in the queue if the queue wraps around
/// the array).
///
/// This is useful for when you want to process many queue elements at
/// once.
template <typename T> T *begin_front(Queue<T> &q);
template <typename T> const T *begin_front(const Queue<T> &q);
template <typename T> T *end_front(Queue<T> &q);
template <typename T> const T *end_front(const Queue<T> &q);

/// Returns the pointers to the contiguous chunks that comprise the given extent. It's struct containing these
/// fields.
template <typename T> struct ChunkExtent {
    T *first_chunk;
    T *second_chunk;
    u32 first_chunk_size;
    u32 second_chunk_size;
};

template <typename T> ChunkExtent<T> get_extent(Queue<T> &q, uint32_t start, uint32_t size);

namespace queue_internal {
// Can only be used to increase the capacity.
template <typename T> void increase_capacity(Queue<T> &q, uint32_t new_capacity) {
    uint32_t end = size(q._data);
    resize(q._data, new_capacity);
    if (q._offset + q._size > end) {
        uint32_t end_items = end - q._offset;
        memmove(begin(q._data) + new_capacity - end_items, begin(q._data) + q._offset, end_items * sizeof(T));
        q._offset += new_capacity - end;
    }
}

template <typename T> void grow(Queue<T> &q, uint32_t min_capacity = 0) {
    uint32_t new_capacity = size(q._data) * 2 + 8;
    if (new_capacity < min_capacity)
        new_capacity = min_capacity;
    increase_capacity(q, new_capacity);
}
} // namespace queue_internal

template <typename T> inline uint32_t size(const Queue<T> &q) { return q._size; }

template <typename T> inline uint32_t space(const Queue<T> &q) { return size(q._data) - q._size; }

template <typename T> void reserve(Queue<T> &q, uint32_t size) {
    if (size > q._size)
        queue_internal::increase_capacity(q, size);
}

template <typename T> inline void push_back(Queue<T> &q, const T &item) {
    if (!space(q))
        queue_internal::grow(q);
    q[q._size++] = item;
}

template <typename T> inline void pop_back(Queue<T> &q) { --q._size; }

template <typename T> inline void push_front(Queue<T> &q, const T &item) {
    if (!space(q))
        queue_internal::grow(q);
    q._offset = (q._offset - 1 + size(q._data)) % size(q._data);
    ++q._size;
    q[0] = item;
}

template <typename T> inline void pop_front(Queue<T> &q) {
    q._offset = (q._offset + 1) % size(q._data);
    --q._size;
}

template <typename T> inline void consume(Queue<T> &q, uint32_t n) {
    q._offset = (q._offset + n) % size(q._data);
    q._size -= n;
}

template <typename T> void push(Queue<T> &q, const T *items, uint32_t n) {
    if (space(q) < n)
        queue_internal::grow(q, size(q) + n);
    const uint32_t size = ::fo::size(q._data);
    const uint32_t insert = (q._offset + q._size) % size;
    uint32_t to_insert = n;
    if (insert + to_insert > size)
        to_insert = size - insert;
    memcpy(begin(q._data) + insert, items, to_insert * sizeof(T));
    q._size += to_insert;
    items += to_insert;
    n -= to_insert;
    memcpy(begin(q._data), items, n * sizeof(T));
    q._size += n;
}

template <typename T> inline T *begin_front(Queue<T> &q) { return begin(q._data) + q._offset; }

template <typename T> inline const T *begin_front(const Queue<T> &q) { return begin(q._data) + q._offset; }

template <typename T> T *end_front(Queue<T> &q) {
    uint32_t end = q._offset + q._size;
    return end > ::fo::size(q._data) ? ::fo::end(q._data) : begin(q._data) + end;
}

template <typename T> const T *end_front(const Queue<T> &q) {
    uint32_t end = q._offset + q._size;
    return end > size(q._data) ? end(q._data) : begin(q._data) + end;
}

template <typename T> ChunkExtent<T> get_extent(Queue<T> &q, uint32_t start, uint32_t count) {
    ChunkExtent<T> e;

    uint32_t first_chunk_start = (q._offset + start) % size(q._data);
    uint32_t first_chunk_end = first_chunk_start + count;

    e.first_chunk = &q._data[first_chunk_start];

    if (first_chunk_end > size(q._data)) {
        e.first_chunk_size = size(q._data) - first_chunk_start;
        e.second_chunk = data(q._data);
        e.second_chunk_size = count - e.first_chunk_size;
    } else {
        e.first_chunk_size = count;
        e.second_chunk = nullptr;
        e.second_chunk_size = 0;
    }

    return e;
}

template <typename T>
Queue<T>::Queue(Allocator &allocator)
    : _data(allocator)
    , _size(0)
    , _offset(0) {}

template <typename T>
Queue<T>::Queue(Queue<T> &&other)
    : _data(std::move(other._data))
    , _size(other._size)
    , _offset(other._offset) {
    other._size = 0;
    other._offset = 0;
}

template <typename T> Queue<T> &Queue<T>::operator=(Queue<T> &&other) {
    if (this != &other) {
        _data = std::move(other._data);
        _size = other._size;
        _offset = other._offset;
        other._size = 0;
        other._offset = 0;
    }
    return *this;
}

template <typename T> inline T &Queue<T>::operator[](uint32_t i) {
    return _data[(i + _offset) % size(_data)];
}

template <typename T> inline const T &Queue<T>::operator[](uint32_t i) const {
    return _data[(i + _offset) % size(_data)];
}
} // namespace fo