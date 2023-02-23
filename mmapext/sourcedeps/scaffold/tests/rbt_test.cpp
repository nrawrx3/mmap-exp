#include <scaffold/array.h>
#include <scaffold/debug.h>
#include <scaffold/memory.h>
#include <scaffold/rbt.bak.h>

// For testing
#include <algorithm>
#include <iostream>
#include <string>

#include <random>

using namespace fo::rbt;
using Tree = RBT<int, int>;

int id = 0;

void print_tree(const RBNode<int, int> *nil, const RBNode<int, int> *node, int node_id) {
    if (node->_kids[LEFT] != nil) {
        std::cout << node_id << " -> " << ++id << ";\n";
        print_tree(nil, node->_kids[LEFT], id);
    }

    std::cout << node_id << " [fillcolor = " << (node->_color == BLACK ? "grey" : "red")
              << ", label = " << node_id << "];\n";

    if (node->_kids[RIGHT] != nil) {
        std::cout << node_id << " -> " << ++id << ";\n";
        print_tree(nil, node->_kids[RIGHT], id);
    }
}

void print_dot_format(const RBNode<int, int> *nil, const RBNode<int, int> *root) {
    std::cout << "digraph rbt {\n";
    id = 1;
    print_tree(nil, root, 1);
    std::cout << "}\n";
}

int main() {

    using namespace fo;

    memory_globals::init();
    {

        constexpr int seed = 0xdeadbeef;
        constexpr int keyend = 1000;
        constexpr int num_inserts = 1000;
        std::default_random_engine dre(seed);
        std::uniform_int_distribution<int> dist(1, keyend);

        Tree tree(memory_globals::default_allocator());
        Array<int> arr_val(memory_globals::default_allocator());
        resize(arr_val, keyend);
        memset(data(arr_val), 0, keyend * sizeof(int));

        Array<Tree::node_type> nodes(memory_globals::default_allocator());
        resize(nodes, num_inserts);

        for (int i = 0; i < num_inserts; ++i) {
            Tree::node_type *node = &nodes[i];
            int k = i;
            int v = dist(dre);
            new (node) Tree::node_type(k, v);
            tree.insert(node);
            arr_val[k] = v;
        }

        for (int i = 1; i < keyend; ++i) {
            int val = tree.get_node(i)->val();
            log_assert(arr_val[i] == val, "");
        }

        print_dot_format(tree.nil_node(), tree._root);
    }

    memory_globals::shutdown();
}
