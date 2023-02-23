#include <stdio.h>

#include <memory.h>
#include <scaffold/scanner.h>

namespace ss = fo::string_stream;
namespace mg = fo::memory_globals;

ss::Buffer read_file(FILE *f) {
    using namespace ss;
    fo::Allocator &alloc = mg::default_allocator();
    Buffer text = Buffer(alloc);
    char c = fgetc(f);
    while (c != EOF) {
        text << c;
        c = fgetc(f);
    }
    return text;
}

int main(int argc, char **argv) {
    mg::init();

    {
        int mode = argc == 2 ? argv[1][0] == 'D' ? scanner::DEFAULT_MODE : scanner::WHOLESTRING_MODE
                             : scanner::WHOLESTRING_MODE;
        ss::Buffer text(std::move(read_file(stdin)));
        scanner::Scanner s(std::move(text), mode);

        int token = scanner::next(s);

        while (token != scanner::EOFS) {
            sds text = scanner::get_token_text(s);
            printf("(%s\t%d\t%d\t%s\t[%c])\n", scanner::desc(token), s.line, s.col, text, token);
            printf("------\n");
            sdsfree(text);
            token = scanner::next(s);
        }
    }
    mg::shutdown();
}
