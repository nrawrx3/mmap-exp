#include "argparse.hpp"

#include <linenoise.h>
#include <mmapext/mmapext.h>
#include <optional>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>
#include <scaffold/const_log.h>
#include <scaffold/scanner.h>
#include <variant>

using namespace std::string_literals;

struct Config {
    std::string filepath;
};

static Config config;

std::aligned_storage_t<sizeof(MmapManager), alignof(MmapManager)> _mmap_manager_store[1];

MmapManager *manager() { return reinterpret_cast<MmapManager *>(_mmap_manager_store); }

struct CmdMapNextChunk {
    static constexpr auto command_string = "map_next";
    static constexpr i32 index = 0;
    int grow_chunks = 0;
};

struct CmdMapNextNChunks {
    static constexpr auto command_string = "map_next_n";
    static constexpr i32 index = 1;
    int n = 0;
};

struct CmdMapUntilExhausted {
    static constexpr auto command_string = "map_until_ex";
    static constexpr i32 index = 2;
    uint64_t chunks_per_increment = 0;
};

struct CmdPrintNumChunksMapped {
    static constexpr i32 index = 3;
    static constexpr auto command_string = "mapped_chunks";
};

using ReplCommand =
    std::variant<CmdMapNextChunk, CmdMapNextNChunks, CmdMapUntilExhausted, CmdPrintNumChunksMapped>;

std::optional<ReplCommand> parse_command(const char *line)
{
    using namespace fo;
    using namespace fo::string_stream;

    auto cmd = ReplCommand(CmdMapNextChunk{});

    string_stream::Buffer line_buf(fo::memory_globals::default_allocator());
    line_buf << line;

    scanner::Scanner s(line_buf, scanner::DEFAULT_MODE);
    Buffer token_text(fo::memory_globals::default_allocator());

    auto tok = scanner::next(s);

    if (tok != scanner::IDENT) {
        PLOGE.printf("expected a command token - have: %s", scanner::desc(tok));
        return {};
    }

    scanner::token_text(s, token_text);

    if (strcmp(c_str(token_text), CmdMapNextChunk::command_string) == 0) {
        auto next = CmdMapNextChunk{};

        tok = scanner::next(s);
        if (tok == scanner::INT) {
            PLOGI.printf("map_next %d", s.current_int);
            next.grow_chunks = s.current_int;
            tok = scanner::next(s);
        }
        
        cmd = next;
    } else if (strcmp(c_str(token_text), CmdMapNextNChunks::command_string) == 0) {
        auto next_n = CmdMapNextNChunks{};
        tok = scanner::next(s);
        if (tok != scanner::INT) {
            PLOGE.printf("expected a number token for map_next_n command, but found: %s", scanner::desc(tok));
            return {};
        }
        next_n.n = s.current_int;
        cmd = next_n;
        tok = scanner::next(s);
    } else if (strcmp(c_str(token_text), CmdMapUntilExhausted::command_string) == 0) {
        auto untilEx = CmdMapUntilExhausted{};
        tok = scanner::next(s);
        if (tok != scanner::INT) {
            PLOGE.printf("expected a number token for map_until_ex command, but found: %s",
                         scanner::desc(tok));
            return {};
        }

        if (s.current_int <= 0) {
            PLOGE.printf("expected positive integer in command map_until_ex <chunks_per_increment>");
            return {};
        }

        untilEx.chunks_per_increment = uint64_t(s.current_int);
        cmd = untilEx;
        tok = scanner::next(s);
    } else if (strcmp(c_str(token_text), CmdPrintNumChunksMapped::command_string) == 0) {
        cmd = CmdPrintNumChunksMapped{};
        tok = scanner::next(s);
    } else {
        PLOGE.printf("unknown command: %s", c_str(token_text));
        return {};
    }

    if (tok != scanner::EOFS) {
        PLOGE.printf("expected end of string but found: '%s'", scanner::desc(tok));
        return {};
    }

    return cmd;
}

void do_map_next(CmdMapNextChunk cmd)
{
    auto man = manager();

    MmapManagerMapNextOptions opts;
    opts.chunks_to_map_next = 1;
    opts.extra_chunks_to_reserve_on_grow = uint64_t(cmd.grow_chunks);
    opts.dont_grow_if_fully_mapped = cmd.grow_chunks <= 0;

    auto res = mmapext_map_next_file_chunk(manager(), opts);
    if (res.error.error_code != MMAPEXT_ERR_NONE) {
        PLOGE.printf("error_code: %d, message: %s, errno: %d, cmd.grow_chunls = %d",
                     res.error.error_code,
                     res.error.error_message,
                     res.error.saved_errno,
                     cmd.grow_chunks);
        return;
    }

    memset(man->address + (man->num_chunks_mapped - 1) * man->_chunk_size, int('a'), manager()->_chunk_size);
}

void do_map_next_til_exhaustion(CmdMapUntilExhausted cmd)
{
    auto man = manager();

    MmapManagerMapNextOptions opts{};
    opts.dont_grow_if_fully_mapped = true;
    opts.chunks_to_map_next = cmd.chunks_per_increment;
    int num_chunks_unmapped = int(man->num_chunks_reserved - man->num_chunks_mapped);
    int64_t increments = ceil_div(int64_t(num_chunks_unmapped), int64_t(cmd.chunks_per_increment));
    auto remaining_chunks = int64_t(num_chunks_unmapped);

    PLOGI.printf("Will map %d chunks, in %d steps", remaining_chunks, increments);

    while (remaining_chunks > 0) {
        opts.chunks_to_map_next = std::min(uint64_t(remaining_chunks), cmd.chunks_per_increment);

        auto res = mmapext_map_next_file_chunk(man, opts);
        if (res.error.error_code != MMAPEXT_ERR_NONE) {
            PLOGE.printf("failed to map next chunk: error_code: %d, error_message: %s",
                         res.error.error_code,
                         res.error.error_message);
            return;
        }

        PLOGI.printf("remaining_chunks = %li", remaining_chunks);

        remaining_chunks -= opts.chunks_to_map_next;
    }

    PLOGI.printf(
        "mapped all chunks (chunks: %lu, size: %lu bytes)", man->num_chunks_mapped, mmapext_mapped_size(man));

    opts.chunks_to_map_next = 1;
    opts.dont_grow_if_fully_mapped = true;
    opts.extra_chunks_to_reserve_on_grow = 0;

    auto res = mmapext_map_next_file_chunk(man, opts);
    if (res.error.error_code == MMAPEXT_ERR_FULLY_MAPPED) {
        PLOGI.printf("As expected, we don't extend the file after address space is full");
    } else {
        PLOGI.printf("reserved_size = %lu, mapped_size = %lu, reserved_chunks = %lu, mapped_chunks = %lu",
                     mmapext_reserved_size(man),
                     mmapext_mapped_size(man),
                     man->num_chunks_reserved,
                     man->num_chunks_mapped);
        PLOGF.printf("Should have received an ERR_FULLY_MAPPED, got code: %d", res.error.error_code);
    }
}

void do_command(ReplCommand cmd)
{
    switch (cmd.index()) {
    case CmdMapNextChunk::index: {
        auto mapNext = std::get<CmdMapNextChunk>(cmd);
        do_map_next(mapNext);
        break;
    }

    case CmdPrintNumChunksMapped::index:
        printf("mapped_chunks = %d\n", manager()->num_chunks_mapped);
        break;

    case CmdMapUntilExhausted::index:
        do_map_next_til_exhaustion(std::get<CmdMapUntilExhausted>(cmd));
        break;

    default:
        PLOGE.printf("unknown command with type index %d", cmd.index());
    }
}

void init_manager()
{
    auto create_opts = MmapManagerCreateOptions{
        .backing_file = config.filepath.c_str(),
        .reserve_existing_file_size = false,
    };

    new (_mmap_manager_store) MmapManager{};
    *manager() = mmapext_create_manager(create_opts);

    if (manager()->error_code != MMAPEXT_ERR_NONE) {
        PLOGE.printf("mmapext_create_manager failed with error code: %d, %s",
                     manager()->error_code,
                     manager()->error_message);
    } else {
        PLOGI.printf("mapped next chunk");
    }

    PLOGI.printf("Initialized first anonymous mapping, pausing for input, press ENTER");
    getchar();
}

void delete_manager()
{
    auto err = mmapext_delete_manager(manager());
    if (err.error_code != MMAPEXT_ERR_NONE) {
        PLOGE.printf("failed to delete manager");
    }
}

int main(int ac, char **av)
{
    fo::memory_globals::init();

    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::verbose, &consoleAppender);

    argparse::ArgumentParser ap("mmap_example");
    ap.add_argument("-f", "--file").help("path to backing file");
    ap.add_argument("-r", "--repl").help("start repl").default_value(false).implicit_value(true);

    try {
        ap.parse_args(ac, av);
    } catch (const std::runtime_error &err) {
        std::cerr << err.what() << std::endl;
        std::exit(1);
    }

    config.filepath = ap.get<std::string>("file");
    PLOGI.printf("backing file = %s", config.filepath.c_str());

    init_manager();

    auto pid = getpid();

    if (ap.get<bool>("repl")) {
        char *line = nullptr;

        std::string prompt = "mmap:"s + std::to_string(pid) + "> "s;

        while ((line = linenoise(prompt.c_str())) != NULL) {
            auto cmd = parse_command(line);
            if (!cmd) {
                linenoiseFree(line);
                continue;
            }

            do_command(cmd.value());

            linenoiseFree(line);
        }
    }

    delete_manager();

    fo::memory_globals::shutdown();
}
