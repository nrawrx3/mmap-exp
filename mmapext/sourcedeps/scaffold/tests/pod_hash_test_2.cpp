// Run as ./tests/pod_hash_test_words < "/usr/share/dict/words"

#include <scaffold/pod_hash.h>
#include <scaffold/pod_hash_usuals.h>

#include <algorithm>
#include <stdio.h>

using namespace fo;

struct WordStore {
    PodHash<char *,
            char *,
            std::add_pointer_t<decltype(fo::usual_hash<char *>)>,
            std::add_pointer_t<decltype(fo::usual_equal<char *>)>>
        string_id;

    WordStore()
        : string_id{ memory_globals::default_allocator(),
                     memory_globals::default_allocator(),
                     usual_hash<char *>,
                     usual_equal<char *> } {}

    char *add_string(char *buffer, uint32_t length) {
        char *str = (char *)memory_globals::default_allocator().allocate(length, 1);
        memcpy(str, buffer, length);
        char *prev_str = set_default(string_id, str, str);
        if (prev_str != str) {
            memory_globals::default_allocator().deallocate(str);
        }
        return prev_str;
    }

    ~WordStore() {
        for (auto entry : string_id) {
            memory_globals::default_allocator().deallocate(entry.value);
        }
    }
};

char buf[1024] = { 0 };
int main() {
    memory_globals::init();
    {
        WordStore w{};
        for (;;) {
            fgets(buf, sizeof(buf), stdin);
            auto len = strlen(buf);
            if (buf[0] == '\n' || buf[0] == '\0') {
                break;
            }
            buf[len - 1] = '\0';
            w.add_string(buf, len);
        }

        WordStore w1{};

        std::swap(w.string_id, w1.string_id);

        for (auto entry : w1.string_id) {
            printf("%s\n", entry.value);
        }
    }
    memory_globals::shutdown();
}
