// Contains a red-black tree based dictionary.

#pragma once

#include <scaffold/array.h>
#include <scaffold/memory.h>
#include <scaffold/temp_allocator.h>

#include <assert.h>
#include <functional>
#include <type_traits>

namespace fo {

// Fwd
template <typename Key, typename T> struct RBNode;

namespace rbt {

enum Color : unsigned { BLACK = 0, RED = 1 };

enum Arrow : unsigned { LEFT = 0, RIGHT = 1 };

template <typename Key, typename T> struct RBNode; // Fwd decl

namespace internal {

// We keep the kid pointers here
template <typename Key, typename T> struct ChildPointers {
    RBNode<Key, T> *_childs[2] = { nullptr, nullptr };
    RBNode<Key, T> *_parent = nullptr;
    Color _color = BLACK;
};

} // namespace internal

/// Represents a node stored in the tree
template <typename Key, typename T> struct RBNode : public internal::ChildPointers<Key, T> {
    Key k;
    T v;

    RBNode() = default;

    RBNode(const Key &k, const T &v)
        : internal::ChildPointers<Key, T>()
        , k(k)
        , v(v) {}

    RBNode(Key &&k, T &&v)
        : internal::ChildPointers<Key, T>()
        , k(std::move(k))
        , v(std::move(v)) {}

    RBNode(const RBNode<Key, T> &) = delete;
    RBNode(RBNode<Key, T> &&) = delete;

    // These two functions are convenience when you want to switch to/from using RBT to std::map or
    // std::unordered_map
    const Key &first() const { return k; }
    const T &second() const { return v; }
    T &second() { return v; }
};

/// Represents a red-black tree based map.
template <typename Key, typename T> struct RBTree {
    using key_type = Key;
    using mapped_type = T;
    using value_type = RBNode<Key, T>;
    using is_less_fn = std::function<bool(const Key &, const Key &)>;

    // This being nullptr denotes moved-from tree
    Allocator *_allocator;

    // Taking the advice from CLRS. Using an explicit node to denote the nil pointer and not using `NULL`.
    internal::ChildPointers<Key, T> *_nil;
    RBNode<Key, T> *_root;

    is_less_fn _less;

    bool _equal(const Key &k1, const Key &k2) const { return !(_less(k1, k2) || _less(k2, k1)); }

    /// Constructs a new RBTree where nodes will be allocated using given `allocator`.
    RBTree(Allocator &allocator, is_less_fn less = std::less<Key>{});
    ~RBTree();

    /// Copy ctor. If used like a copy constructor i.e you don't provide an allocator, the new tree will use
    /// the allocator used by `o`
    RBTree(const RBTree &o, Allocator *allocator = nullptr);

    /// Copy assignment.
    RBTree &operator=(const RBTree &o);

    /// Move constructor. The moved-from RBTree must not be operated on any further (except calling its
    /// destructor).
    RBTree(RBTree &&o);

    /// Move assignment.
    RBTree &operator=(RBTree &&o);
};

// Most of the traversal algorithms just need to go from node to node and return a pointer to some node.
// Whether it's a pointer to const node or non-const is not relevant. I could write a function once that takes
// a const pointer argument and then `const_cast` to remove the const. Or I could template the whole 'Node'
// type. But a middle ground is to template on whether the const-ness of the RBNode object itself, and
// implement the traversalk functions templated on Key, T, and is_const.
template <template <class, class> typename TreeOrNode, typename Key, typename T, bool is_const>
using KT = typename std::conditional<is_const, const TreeOrNode<Key, T>, TreeOrNode<Key, T>>::type;

/// The iterator is bidirectional, but not random-access.
template <typename Key, typename T, bool is_const> struct Iterator {
    using TreeType = KT<RBTree, Key, T, is_const>;
    using NodeType = KT<RBNode, Key, T, is_const>;

    TreeType *_tree;
    NodeType *_node;

    Iterator() = delete;
    Iterator(TreeType &tree, NodeType *node)
        : _tree(&tree)
        , _node(node) {}

    Iterator(const Iterator &) = default;
    Iterator(Iterator &&) = default;
    Iterator &operator=(const Iterator &) = default;
    Iterator &operator=(Iterator &&) = default;

    // Both ++ and -- have a time complexity of O(log n) where n is number of currently in tree.

    Iterator &operator++();
    Iterator operator++(int);

    Iterator &operator--();
    Iterator operator--(int);

    NodeType *operator->() const { return _node; }
    NodeType &operator*() const { return *_node; }

    template <bool other_const> bool operator==(const Iterator<Key, T, other_const> &o) const {
        return o._node == _node;
    }

    template <bool other_const> bool operator!=(const Iterator<Key, T, other_const> &o) const {
        return o._node != _node;
    }
};

} // namespace rbt

// Default Implementation

namespace rbt {

template <typename Key, typename T>
inline bool is_nil_node(const RBTree<Key, T> &rbt, const RBNode<Key, T> *node) {
    return static_cast<const internal::ChildPointers<Key, T> *>(node) == rbt._nil;
}

template <typename Key, typename T>
u32 count_nodes(const RBTree<Key, T> &tree, const RBNode<Key, T> *current_node, u32 parent_count) {
    if (!is_nil_node(tree, current_node)) {
        parent_count += 1 + count_nodes(tree, current_node->_childs[0], 0);
        return count_nodes(tree, current_node->_childs[1], parent_count);
    }
    return parent_count;
}

template <typename Key, typename T> u32 count_nodes(const RBTree<Key, T> &tree) {
    return count_nodes(tree, tree._root, 0);
}

namespace internal {

template <typename Key, typename T> void delete_all_nodes(RBTree<Key, T> &tree, bool delete_nil_node) {
    if (tree._allocator == nullptr) {
        return;
    }

    if (is_nil_node(tree, tree._root)) {
        if (delete_nil_node) {
            make_delete(*tree._allocator, tree._nil);
            tree._root = nullptr;
            tree._nil = nullptr;
        }
        return;
    }

    TempAllocator1024 ta(memory_globals::default_allocator());
    Array<RBNode<Key, T> *> stack(ta);
    reserve(stack, 512 / sizeof(RBNode<Key, T> *));

    push_back(stack, tree._root);

    while (size(stack) != 0) {
        RBNode<Key, T> *p = back(stack);
        pop_back(stack);
        for (u32 i = 0; i < 2; ++i) {
            if (!is_nil_node(tree, p->_childs[i])) {
                push_back(stack, p->_childs[i]);
            }
        }
        make_delete(*tree._allocator, p);
    }

    if (delete_nil_node) {
        make_delete(*tree._allocator, tree._nil);
        tree._root = nullptr;
        tree._nil = nullptr;
    }
}

template <typename Key, typename T> void copy_tree(RBTree<Key, T> &tree, const RBTree<Key, T> &other) {
    if (is_nil_node(other, other._root)) {
        return;
    }

    tree._root = make_new<RBNode<Key, T>>(*tree._allocator, other._root->k, other._root->v);

    // We maintain the recursion stack ourselves.

    fo::TempAllocator1024 ta(fo::memory_globals::default_allocator());

    fo::Array<RBNode<Key, T> *> others_stack(ta);
    fo::Array<RBNode<Key, T> *> wips_stack(ta);

    fo::push_back(others_stack, other._root);
    fo::push_back(wips_stack, tree._root);

    while (fo::size(others_stack) != 0) {
        auto wips_top = fo::back(wips_stack);
        auto others_top = fo::back(others_stack);

        fo::pop_back(wips_stack);
        fo::pop_back(others_stack);

        wips_top->_color = others_top->_color;

        for (u32 i = 0; i < 2; ++i) {
            if (rbt::is_nil_node(other, others_top->_childs[i])) {
                wips_top->_childs[i] = static_cast<RBNode<Key, T> *>(tree._nil);
            } else {
                // Create the child node
                wips_top->_childs[i] = make_new<RBNode<Key, T>>(
                    *tree._allocator, others_top->_childs[i]->k, others_top->_childs[i]->v);
                fo::push_back(others_stack, others_top->_childs[i]);
                fo::push_back(wips_stack, wips_top->_childs[i]);
            }
        }
    }
}

template <typename Key, typename T, bool is_const>
KT<RBNode, Key, T, is_const> *find(KT<RBTree, Key, T, is_const> &rbt, const Key &k) {
    auto cur_node = rbt._root;
    while (!is_nil_node(rbt, cur_node)) {
        if (rbt._equal(cur_node->k, k)) {
            return cur_node;
        }
        if (rbt._less(cur_node->k, k)) {
            cur_node = cur_node->_childs[1];
        } else {
            cur_node = cur_node->_childs[0];
        }
    }

    return cur_node;
}

template <typename Key, typename T> RBNode<Key, T> *nil_node(RBTree<Key, T> &t) {
    return static_cast<RBNode<Key, T> *>(t._nil);
}

template <int left, int right, typename Key, typename T> void rotate(RBTree<Key, T> &t, RBNode<Key, T> *x) {
    auto y = x->_childs[right];
    x->_childs[right] = y->_childs[left];
    if (!is_nil_node(t, y->_childs[left])) {
        y->_childs[left]->_parent = x;
    }
    y->_parent = x->_parent;
    if (is_nil_node(t, x->_parent)) {
        t._root = y;
    } else if (x == x->_parent->_childs[left]) {
        x->_parent->_childs[left] = y;
    } else {
        x->_parent->_childs[right] = y;
    }
    y->_childs[left] = x;
    x->_parent = y;
}

template <typename Key, typename T>
void transplant(RBTree<Key, T> &t, RBNode<Key, T> *n1, RBNode<Key, T> *n2) {
    if (n1 == t._root) {
        t._root = n2;
    } else if (n1->_parent->_childs[0] == n1) {
        n1->_parent->_childs[0] = n2;
    } else {
        n1->_parent->_childs[1] = n2;
    }
    n2->_parent = n1->_parent;
}

template <int left, int right, typename Key, typename T>
RBNode<Key, T> *insert_fix(RBTree<Key, T> &t, RBNode<Key, T> *z) {
    auto y = z->_parent->_parent->_childs[right];
    if (y->_color == RED) {
        z->_parent->_color = BLACK;
        y->_color = BLACK;
        z->_parent->_parent->_color = RED;
        z = z->_parent->_parent;
    } else {
        if (z == z->_parent->_childs[right]) {
            z = z->_parent;
            rotate<left, right>(t, z);
        }
        z->_parent->_color = BLACK;
        z->_parent->_parent->_color = RED;
        rotate<right, left>(t, z->_parent->_parent);
    }
    return z;
}

template <int left, int right, typename Key, typename T>
RBNode<Key, T> *remove_fix(RBTree<Key, T> &t, RBNode<Key, T> *x) {
    auto w = x->_parent->_childs[right];
    if (w->_color == RED) {
        w->_color = BLACK;
        x->_parent->_color = RED;
        rotate<left, right>(t, x->_parent);
        w = x->_parent->_childs[right];
    }
    if (w->_childs[left]->_color == BLACK && w->_childs[right]->_color == BLACK) {
        w->_color = RED;
        x = x->_parent;
    } else {
        if (w->_childs[right]->_color == BLACK) {
            w->_childs[left]->_color = BLACK;
            w->_color = RED;
            rotate<right, left>(t, w);
            w = x->_parent->_childs[right];
        }
        w->_color = x->_parent->_color;
        x->_parent->_color = BLACK;
        w->_childs[right]->_color = BLACK;
        rotate<left, right>(t, x->_parent);
        x = t._root;
    }
    return x;
}

template <typename Key, typename T, bool is_const>
KT<RBNode, Key, T, is_const> *min_node_of_subtree(KT<RBTree, Key, T, is_const> &t,
                                                  KT<RBNode, Key, T, is_const> *node) {
    if (is_nil_node(t, node)) {
        return node;
    }
    while (!is_nil_node(t, node->_childs[LEFT])) {
        node = node->_childs[LEFT];
    }
    return node;
}

template <typename Key, typename T, bool is_const>
KT<RBNode, Key, T, is_const> *max_node_of_subtree(KT<RBTree, Key, T, is_const> &t,
                                                  KT<RBNode, Key, T, is_const> *node) {
    if (is_nil_node(t, node)) {
        return node;
    }
    while (!is_nil_node(t, node->_childs[RIGHT])) {
        node = node->_childs[RIGHT];
    }
    return node;
}

// Returns the next in-order node for the given `node`. Given node must not be pointing to `nil`.
template <typename Key, typename T, bool is_const>
KT<RBNode, Key, T, is_const> *next_inorder_node(KT<RBTree, Key, T, is_const> &t,
                                                KT<RBNode, Key, T, is_const> *node) {
    // Right subtree is nil? Follow parent pointers as long as we are right node.
    if (is_nil_node(t, node->_childs[RIGHT])) {
        while (node->_parent->_childs[RIGHT] == node) {
            node = node->_parent;
        }
        return node->_parent;
    }
    // Get the minimum node of right subtree.
    auto r = min_node_of_subtree<Key, T, is_const>(t, node->_childs[RIGHT]);
    return r;
}

// Returns the previous in-order node for given `node`. Given node must not be pointing to `nil`.
template <typename Key, typename T, bool is_const>
KT<RBNode, Key, T, is_const> *prev_inorder_node(KT<RBTree, Key, T, is_const> &t,
                                                KT<RBNode, Key, T, is_const> *node) {
    if (is_nil_node(t, node->_childs[LEFT])) {
        while (node->_parent->_childs[LEFT] == node) {
            node = node->_parent;
        }
        return node->_parent;
    }
    // Not leaf node. Get the maximum of left subtree
    auto l = max_node_of_subtree<Key, T, is_const>(t, node->_childs[LEFT]);
    return l;
}

} // namespace internal
} // namespace rbt

namespace rbt {

template <typename Key, typename T>
RBTree<Key, T>::RBTree(Allocator &allocator, RBTree<Key, T>::is_less_fn less)
    : _less(less) {
    _allocator = &allocator;
    _nil = make_new<internal::ChildPointers<Key, T>>(*_allocator);
    _nil->_childs[LEFT] = static_cast<RBNode<Key, T> *>(_nil);
    _nil->_childs[RIGHT] = static_cast<RBNode<Key, T> *>(_nil);
    _nil->_parent = static_cast<RBNode<Key, T> *>(_nil);
    // _nil->_parent = nullptr;
    // _nil->_childs[LEFT] = nullptr;
    // _nil->_childs[RIGHT] = nullptr;
    _root = static_cast<RBNode<Key, T> *>(_nil);
}

template <typename Key, typename T>
RBTree<Key, T>::RBTree(const RBTree &other, Allocator *allocator)
    : _less(other._less) {
    _allocator = allocator ? allocator : other._allocator;
    _nil = make_new<internal::ChildPointers<Key, T>>(*_allocator);
    _nil->_childs[LEFT] = static_cast<RBNode<Key, T> *>(_nil);
    _nil->_childs[RIGHT] = static_cast<RBNode<Key, T> *>(_nil);
    _nil->_parent = static_cast<RBNode<Key, T> *>(_nil);
    _root = static_cast<RBNode<Key, T> *>(_nil);

    rbt::internal::copy_tree(*this, other);
}

template <typename Key, typename T> RBTree<Key, T>::RBTree(RBTree &&other) {
    _allocator = other._allocator;
    _root = other._root;
    _nil = other._nil;
    _less = std::move(other._less);
    other._allocator = nullptr;
}

template <typename Key, typename T> RBTree<Key, T> &RBTree<Key, T>::operator=(const RBTree &other) {
    if (this == &other) {
        return *this;
    }

    assert(_allocator != nullptr && "Cannot copy into moved-from/deleted tree");

    rbt::internal::delete_all_nodes(*this, false);
    _root = reinterpret_cast<RBNode<Key, T> *>(_nil);
    _less = other._less;

    rbt::internal::copy_tree(*this, other);

    return *this;
}

template <typename Key, typename T> RBTree<Key, T> &RBTree<Key, T>::operator=(RBTree &&other) {
    if (this == &other) {
        return *this;
    }

    rbt::internal::delete_all_nodes(*this, true);

    _allocator = other._allocator;
    _root = other._root;
    _nil = other._nil;
    _less = std::move(other._less);

    other._allocator = nullptr;

    return *this;
}

template <typename Key, typename T> RBTree<Key, T>::~RBTree() {
    if (_allocator) {
        rbt::internal::delete_all_nodes(*this, true);
        _allocator = nullptr;
    }
}

template <typename Key, typename T> auto begin(const RBTree<Key, T> &t) {
    return Iterator<Key, T, true>(t, internal::min_node_of_subtree<Key, T, true>(t, t._root));
}

template <typename Key, typename T> auto begin(RBTree<Key, T> &t) {
    return Iterator<Key, T, false>(t, internal::min_node_of_subtree<Key, T, false>(t, t._root));
}

template <typename Key, typename T> auto end(const RBTree<Key, T> &t) {
    return Iterator<Key, T, true>(t, static_cast<const RBNode<Key, T> *>(t._nil));
}

template <typename Key, typename T> auto end(RBTree<Key, T> &t) {
    return Iterator<Key, T, false>(t, static_cast<RBNode<Key, T> *>(t._nil));
}

template <typename Key, typename T, bool is_const>
Iterator<Key, T, is_const> &Iterator<Key, T, is_const>::operator++() {
    _node = internal::next_inorder_node<Key, T, is_const>(*_tree, _node);
    return *this;
}

template <typename Key, typename T, bool is_const>
Iterator<Key, T, is_const> Iterator<Key, T, is_const>::operator++(int) {
    Iterator<Key, T, is_const> saved(*this);
    this->operator++();
    return saved;
}

template <typename Key, typename T, bool is_const>
Iterator<Key, T, is_const> &Iterator<Key, T, is_const>::operator--() {
    if (is_nil_node(*_tree, _node)) {
        _node = internal::max_node_of_subtree<Key, T, is_const>(*_tree, _tree->_root);
    } else {
        _node = internal::prev_inorder_node<Key, T, is_const>(*_tree, _node);
    }
    return *this;
}

template <typename Key, typename T, bool is_const>
Iterator<Key, T, is_const> Iterator<Key, T, is_const>::operator--(int) {
    Iterator<Key, T, is_const> saved(*this);
    this->operator--();
    return saved;
}

template <typename Key, typename T> constexpr size_t node_size(const RBTree<Key, T> &) {
    return sizeof(RBNode<Key, T>);
}

/// Represents the result of `get` and `set` operations.
template <typename Key, typename T, bool is_const> struct Result {
    bool key_was_present = false;
    Iterator<Key, T, is_const> i;
};

/// Deletes all nodes in the RBTree
template <typename Key, typename T> void clear(RBTree<Key, T> &rbt) {
    internal::delete_all_nodes(rbt, false);
}

template <typename Key, typename T> Result<Key, T, false> get(RBTree<Key, T> &rbt, const Key &k) {
    auto node = internal::find<Key, T, false>(rbt, k);
    if (is_nil_node(rbt, node)) {
        return Result<Key, T, false>{ false, end(rbt) };
    }
    return Result<Key, T, false>{ true, Iterator<Key, T, false>(rbt, node) };
}

template <typename Key, typename T> Result<Key, T, true> get_const(const RBTree<Key, T> &rbt, const Key &k) {
    auto node = internal::find<Key, T, true>(rbt, k);
    if (is_nil_node(rbt, node)) {
        return Result<Key, T, true>{ false, end(rbt) };
    }
    return Result<Key, T, true>{ true, Iterator<Key, T, true>(rbt, node) };
}

template <typename Key, typename T> Result<Key, T, true> get(const RBTree<Key, T> &rbt, Key k) {
    return get_const(rbt, k);
}

template <typename Key, typename T> Result<Key, T, false> set(RBTree<Key, T> &rbt, Key k, T v) {
    if (is_nil_node(rbt, rbt._root)) {
        rbt._root = make_new<RBNode<Key, T>>(*rbt._allocator, std::move(k), std::move(v));
        rbt._root->_childs[0] = static_cast<RBNode<Key, T> *>(rbt._nil);
        rbt._root->_childs[1] = static_cast<RBNode<Key, T> *>(rbt._nil);
        rbt._root->_parent = static_cast<RBNode<Key, T> *>(rbt._nil);
        return Result<Key, T, false>{ false, Iterator<Key, T, false>(rbt, rbt._root) };
    }

    Result<Key, T, false> result{ false, end(rbt) };

    auto cur = rbt._root;
    auto par = rbt._root;
    Arrow dir = LEFT;
    while (!is_nil_node(rbt, cur)) {
        assert(cur != nullptr);
        par = cur;
        if (rbt._less(k, cur->k)) {
            cur = cur->_childs[LEFT];
            dir = LEFT;
        } else if (rbt._less(cur->k, k)) {
            cur = cur->_childs[RIGHT];
            dir = RIGHT;
        } else {
            cur->k = std::move(k);
            cur->v = std::move(v);
            result.key_was_present = true;
            result.i = Iterator<Key, T, false>(rbt, cur);

            return result;
        }
    }

    auto n = make_new<RBNode<Key, T>>(*rbt._allocator, std::move(k), std::move(v));
    result.i = Iterator<Key, T, false>(rbt, n);
    n->_color = RED;
    n->_parent = par;
    n->_childs[LEFT] = static_cast<RBNode<Key, T> *>(rbt._nil);
    n->_childs[RIGHT] = static_cast<RBNode<Key, T> *>(rbt._nil);
    par->_childs[dir] = n;

    // bottom-up fix
    while (n->_parent->_color == RED) {
        if (n->_parent == n->_parent->_parent->_childs[LEFT]) {
            n = internal::insert_fix<LEFT, RIGHT>(rbt, n);
        } else {
            n = internal::insert_fix<RIGHT, LEFT>(rbt, n);
        }
    }

    rbt._root->_color = BLACK;

    return result;
}

// xyspoon: Too much duplication of code. Clean this up. Make set and set_default use the same function before
// fixup.
template <typename Key, typename T> Result<Key, T, false> set_default(RBTree<Key, T> &rbt, Key k, T v) {
    if (is_nil_node(rbt, rbt._root)) {
        rbt._root = make_new<RBNode<Key, T>>(*rbt._allocator, std::move(k), std::move(v));
        rbt._root->_childs[0] = reinterpret_cast<RBNode<Key, T> *>(rbt._nil);
        rbt._root->_childs[1] = reinterpret_cast<RBNode<Key, T> *>(rbt._nil);
        rbt._root->_parent = reinterpret_cast<RBNode<Key, T> *>(rbt._nil);

        return Result<Key, T, false>{ false, Iterator<Key, T, false>(rbt, rbt._root) };
    }

    Result<Key, T, false> result{ false, end(rbt) };

    auto cur = rbt._root;
    auto par = rbt._root;
    Arrow dir = LEFT;
    while (!is_nil_node(rbt, cur)) {
        assert(cur != nullptr);
        par = cur;
        if (rbt._less(k, cur->k)) {
            cur = cur->_childs[LEFT];
            dir = LEFT;
        } else if (k > cur->k) {
            cur = cur->_childs[RIGHT];
            dir = RIGHT;
        } else {
            // The only place in the code where it's different from `set`. Yuck. Factor this.
            result.key_was_present = true;
            result.i = Iterator<Key, T, false>(rbt, cur);
            return result;
        }
    }

    auto n = make_new<RBNode<Key, T>>(*rbt._allocator, std::move(k), std::move(v));
    result.i = Iterator<Key, T, false>(rbt, n);
    n->_color = RED;
    n->_parent = par;
    n->_childs[LEFT] = static_cast<RBNode<Key, T> *>(rbt._nil);
    n->_childs[RIGHT] = static_cast<RBNode<Key, T> *>(rbt._nil);
    par->_childs[dir] = n;

    // bottom-up fix
    while (n->_parent->_color == RED) {
        if (n->_parent == n->_parent->_parent->_childs[LEFT]) {
            n = internal::insert_fix<LEFT, RIGHT>(rbt, n);
        } else {
            n = internal::insert_fix<RIGHT, LEFT>(rbt, n);
        }
    }
    rbt._root->_color = BLACK;
    return result;
}

// Removes the node with the given key.
template <typename Key, typename T> Result<Key, T, false> remove(RBTree<Key, T> &t, Key k) {
    auto n = internal::find<Key, T, false>(t, k);

    if (is_nil_node(t, n)) {
        return Result<Key, T, false>{ false, end(t) };
    }

    assert(t._equal(n->k, k));

    auto y = n;
    auto orig_color = n->_color;
    RBNode<Key, T> *x;
    if (is_nil_node(t, n->_childs[LEFT])) {
        x = n->_childs[RIGHT];
        transplant(t, n, n->_childs[RIGHT]);
    } else if (is_nil_node(t, n->_childs[RIGHT])) {
        x = n->_childs[LEFT];
        transplant(t, n, n->_childs[LEFT]);
    } else {
        // minimum of n->_childs[RIGHT]
        auto min = n->_childs[RIGHT];
        while (!is_nil_node(t, min->_childs[LEFT])) {
            min = min->_childs[LEFT];
        }
        y = min;
        orig_color = y->_color;
        x = y->_childs[RIGHT];
        if (y->_parent == n) {
            x->_parent = y;
        } else {
            transplant(t, y, y->_childs[RIGHT]);
            y->_childs[RIGHT] = n->_childs[RIGHT];
            y->_childs[RIGHT]->_parent = y;
        }
        transplant(t, n, y);
        y->_childs[LEFT] = n->_childs[LEFT];
        y->_childs[LEFT]->_parent = y;
        y->_color = n->_color;
    }

    if (orig_color != RED) {
        // remove fix
        while (x != t._root && x->_color == BLACK) {
            if (x == x->_parent->_childs[LEFT]) {
                x = internal::remove_fix<LEFT, RIGHT>(t, x);
            } else {
                x = internal::remove_fix<RIGHT, LEFT>(t, x);
            }
        }
        x->_color = BLACK;
    }

    // Delete the node
    make_delete(*t._allocator, n);

    return Result<Key, T, false>{ true, end(t) };
}

} // namespace rbt

} // namespace fo
