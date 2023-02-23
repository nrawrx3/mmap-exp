#pragma once

#include <scaffold/non_pods.h>

namespace fo {

template <typename Key, typename Value> auto size(const OrderedMap<Key, Value> &m) {
    return rbt::count_nodes(m._rbt);
}

// returns iterator
template <typename Key, typename Value> auto get(const OrderedMap<Key, Value> &m, const Key &k) {
    return rbt::get(m._rbt, k).i;
}

// returns iterator
template <typename Key, typename Value> auto get(OrderedMap<Key, Value> &m, const Key &k) {
    return rbt::get(m._rbt, k).i;
}

// returns iterator
template <typename Key, typename Value> auto set(OrderedMap<Key, Value> &m, Key k, Value v) {
    return rbt::set(m._rbt, std::move(k), std::move(v)).i;
}

// returns bool, indicating if removal did take place
template <typename Key, typename Value> bool remove(OrderedMap<Key, Value> &m, Key k) {
    return rbt::remove(m._rbt, std::move(k)).key_was_present;
}

// returns iterator
template <typename Key, typename Value>
auto set_default(OrderedMap<Key, Value> &m, Key k, Value default_value) {
    return rbt::set_default(m._rbt, k, std::move(default_value)).i;
}

template <typename Key, typename Value> Value &OrderedMap<Key, Value>::operator[](const Key &k) {
    static_assert(std::is_default_constructible<Value>::value, "");
    return set_default(*this, k, Value())->v;
}

template <typename Key, typename Value> const Value &OrderedMap<Key, Value>::operator[](const Key &k) const {
    static_assert(std::is_default_constructible<Value>::value, "");
    return set_default(*this, k, Value())->v;
}

#if 0

// Ctor
template <typename Key, typename Value>
OrderedMap<Key, Value>::OrderedMap(fo::Allocator &allocator)
    : _rbt(allocator) {}

// Dtor
template <typename Key, typename Value> OrderedMap<Key, Value>::~OrderedMap() {}

#endif

// # Begin and end iterators

template <typename Key, typename Value> auto begin(OrderedMap<Key, Value> &m) { return m.begin(); }
template <typename Key, typename Value> auto end(OrderedMap<Key, Value> &m) { return m.end(); }
template <typename Key, typename Value> auto begin(const OrderedMap<Key, Value> &m) { return m.begin(); }
template <typename Key, typename Value> auto end(const OrderedMap<Key, Value> &m) { return m.end(); }

} // namespace fo
