#include <limits>
#include <scaffold/ordered_map.h>
#include <scaffold/string_stream.h>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using namespace fo;
using namespace string_stream;

template <typename Key, typename T> class RbtDebugPrint {
  public:
    const rbt::RBTree<Key, T> &_t;
    Buffer _ss;
    int global_id = 0;

    RbtDebugPrint(const rbt::RBTree<Key, T> &t)
        : _t(t)
        , _ss(memory_globals::default_allocator()) {}

    void rbt_debug_print(const rbt::RBNode<Key, T> *n, int node_id) {
        if (!is_nil_node(_t, n->_childs[rbt::LEFT])) {
            printf(_ss, "%i -> %i;\n", node_id, ++global_id);
            rbt_debug_print(n->_childs[rbt::LEFT], global_id);
        }

        printf(_ss,
               "%i [style = filled, fillcolor = %s, label = \"%u_%s\"];\n",
               node_id,
               n->_color == rbt::BLACK ? "grey" : "red",
               n->k,
               n->v.c_str());

        if (!is_nil_node(_t, n->_childs[rbt::RIGHT])) {
            printf(_ss, "%i -> %i;\n", node_id, ++global_id);
            rbt_debug_print(n->_childs[rbt::RIGHT], global_id);
        }
    }

    void rbt_debug_print() {
        _ss << "digraph rbt {\n";
        rbt_debug_print(_t._root, global_id);
        _ss << "}\n";
    }
};

struct FileContent {
    std::vector<u32> numbers;
    std::vector<std::string> strings;
};

FileContent read_number_list(const char *file) {
    FILE *f = fopen(file, "r");

    if (f == nullptr) {
        fprintf(stderr, "Could not open file: %s\n", file);
        exit(EXIT_FAILURE);
    }

    std::vector<u32> numbers;
    numbers.reserve(1000);

    std::vector<std::string> strings;
    strings.reserve(1000);

    while (!feof(f)) {
        char buf[1024] = {};
        fgets(buf, sizeof(buf), f);
        size_t len = strlen(buf);
        if (len == 0) {
            continue;
        }
        if (buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        }
        if (strlen(buf) == 0) {
            continue;
        }

        char *end = nullptr;
        unsigned long number = strtoul(buf, &end, 10);
        if (*end != ' ') {
            fprintf(stderr, "Bad number: %lu, end = '%c', read = '%s'\n", number, end[0], buf);
        }

        assert(std::numeric_limits<u32>::max() >= number);
        numbers.push_back((u32)number);
        strings.push_back(std::string(end + 1));
    }
    return FileContent{std::move(numbers), std::move(strings)};
}

struct RBTandMap {
    rbt::RBTree<u32, std::string> rbt;
    std::unordered_map<u32, std::string> map;
    std::vector<u32> read_keys;
    u32 unique_count;
};

RBTandMap read_file_into_rbt(const char *file) {
    FileContent fc = read_number_list(file);

    rbt::RBTree<u32, std::string> t(memory_globals::default_allocator());
    std::unordered_map<u32, std::string> map;

    for (size_t i = 0; i < fc.numbers.size(); ++i) {
        u32 n = fc.numbers[i];
        std::string v = fc.strings[i];
        rbt::set(t, n, v);
        map[n] = std::move(v);
    }

    return RBTandMap{std::move(t), std::move(map), std::move(fc.numbers), (u32)fc.numbers.size()};
}

void must_have_each_key(RBTandMap &context) {
    for (auto &kv : context.map) {
        auto find_res = rbt::get(context.rbt, kv.first);
        if (!find_res.key_was_present) {
            fprintf(stderr, "must_have_each_key failed for key: %u\n", kv.first);
            continue;
        }
        if (find_res.i->v != kv.second) {
            fprintf(stderr, "Key value mismatch for key: %u\n", kv.first);
        }
    }
}

void test_must_have_each_key() {
    const char *file = SOURCE_DIR "/rbt_keys.txt";
    auto context = read_file_into_rbt(file);
    must_have_each_key(context);

    std::set<u32> unique_keys(context.read_keys.begin(), context.read_keys.end());

    for (u32 n : unique_keys) {
        if (n % 3 == 0) {
            auto res = rbt::remove(context.rbt, n);
            auto it = context.map.find(n);
            context.map.erase(it);
            // printf("Removing: %u\n", n);
            // rbt::internal::debug_print(context.rbt, context.rbt._root);
            // puts("\n");

            assert(rbt::is_nil_node(context.rbt, context.rbt._nil->_childs[0]));
            assert(rbt::is_nil_node(context.rbt, context.rbt._nil->_childs[1]));
        }
    }
    must_have_each_key(context);

    printf("%s - success\n", __PRETTY_FUNCTION__);
}

void test_iterators_sorted() {
    const char *file = SOURCE_DIR "/rbt_keys.txt";
    auto context = read_file_into_rbt(file);
    {

        i32 prev = -9999;

        for (auto &node : context.rbt) {
            // fprintf(stderr, "%u, ", node.k);
            assert((i32)node.k > prev);
            prev = (i32)node.k;
        }
        // puts("\n");
    }

    {

        u32 prev = std::numeric_limits<u32>::max();
        auto it = end(context.rbt);
        --it;

        for (auto b = begin(context.rbt); it != b; --it) {
            // fprintf(stderr, "%u, ", it->k);
            assert(it->k < prev);
            prev = it->k;
        }
        // puts("\n");
    }

    printf("%s - success\n", __PRETTY_FUNCTION__);
}

void test_copy_and_move() {
    const char *file = SOURCE_DIR "/rbt_keys.txt";
    auto context = read_file_into_rbt(file);

    rbt::RBTree<u32, std::string> copied_rbt(memory_globals::default_allocator());

    // Get the max number
    auto it = end(context.rbt);
    --it;

    assert(it != end(context.rbt));

    const u32 max_number = it->k;

    rbt::set(copied_rbt, max_number + 100, std::string("Kyuss"));
    rbt::set(copied_rbt, max_number + 200, std::string("Nine Inch Nails"));
    rbt::set(copied_rbt, max_number + 300, std::string("Converge"));

    assert(rbt::get_const(copied_rbt, max_number + 100).i->v == std::string("Kyuss"));
    assert(rbt::get_const(copied_rbt, max_number + 200).i->v == std::string("Nine Inch Nails"));
    assert(rbt::get_const(copied_rbt, max_number + 300).i->v == std::string("Converge"));

    // Copy assign rbt that was read from file
    copied_rbt = context.rbt;

    assert(rbt::get_const(copied_rbt, max_number + 100).i == end(copied_rbt));
    assert(rbt::get_const(copied_rbt, max_number + 200).i == end(copied_rbt));
    assert(rbt::get_const(copied_rbt, max_number + 300).i == end(copied_rbt));

    // Test they have same keys
    for (auto &node : context.rbt) {
        auto res = rbt::get_const(copied_rbt, node.k);
        assert(res.key_was_present);
        assert(res.i->k == node.k);
        assert(res.i->v == node.v);
    }

    // Delete original tree that was read from file
    context.rbt.~RBTree();
    context.read_keys.clear();
    context.map.clear();

    // Read from file again
    auto new_context = read_file_into_rbt(file);

    // Test they have same keys, again
    for (auto &node : new_context.rbt) {
        auto res = rbt::get_const(copied_rbt, node.k);
        assert(res.key_was_present);
        assert(res.i->k == node.k);
        assert(res.i->v == node.v);
    }

    // Now move assign into copied_rbt
    copied_rbt = std::move(new_context.rbt);

    auto new_context2 = read_file_into_rbt(file);

    for (auto &node : new_context2.rbt) {
        auto res = rbt::get_const(copied_rbt, node.k);
        assert(res.key_was_present);
        assert(res.i->k == node.k);
        assert(res.i->v == node.v);
    }

    printf("%s - success\n", __PRETTY_FUNCTION__);
}

void print_graph() {
    const char *file = SOURCE_DIR "/rbt_keys.txt";
    auto context = read_file_into_rbt(file);
    RbtDebugPrint<u32, std::string> debug_print(context.rbt);
    debug_print.rbt_debug_print();
    FILE *f = fopen(SOURCE_DIR "/rbt_print.dot", "w");
    if (!f) {
        fprintf(stderr, "Failed to create/open output file\n");
        exit(EXIT_FAILURE);
    }
    fwrite(data(debug_print._ss), size(debug_print._ss), 1, f);
    fclose(f);
}

void ordered_map_test() {
    // OrderedMap tests
    using Map = OrderedMap<std::string, unsigned>;

    Map word_count(memory_globals::default_allocator());

    // clang-format off
    std::vector<std::string> word_list{
        "I", "Got", "That", "Summertime", "Summertime", "Sadness",
        "Sa", "Sa", "Summertime",
        "Summertime", "Sadness",
        "Got", "That", "Summertime",
        "Summertime", "Sadness"
    };
    // clang-format on

    for (auto &s : word_list) {
        auto &count = set_default(word_count, s, 0u)->v;
        count = count + 1;
    }

    for (auto s = cbegin(word_count), e = cend(word_count); s != e; ++s) {
        printf("count[%s] = %u\n", s->k.c_str(), s->v);
    }
}

int main() {
    memory_globals::init();
    {
        test_must_have_each_key();
        print_graph();
        test_iterators_sorted();
        test_copy_and_move();

        ordered_map_test();
    }
    memory_globals::shutdown();
}
