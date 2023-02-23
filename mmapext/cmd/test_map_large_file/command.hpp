#include <mmapext/mmapext.h>
#include <plog/Log.h>
#include <scaffold/scanner.h>
#include <scaffold/types.h>

#include <optional>
#include <variant>

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

void do_map_next(CmdMapNextChunk cmd, MmapManager *manager)
{

    MmapManagerMapNextOptions opts;
    opts.chunks_to_map_next = 1;
    opts.extra_chunks_to_reserve_on_grow = uint64_t(cmd.grow_chunks);
    opts.dont_grow_if_fully_mapped = cmd.grow_chunks <= 0;

    auto err = mmapext_map_next_file_chunk(manager, opts);
    if (err.error_code != MMAPEXT_ERR_NONE) {
        PLOGE.printf("error_code: %d, message: %s, errno: %d, cmd.grow_chunls = %d",
                     err.error_code,
                     err.error_message,
                     err.saved_errno,
                     cmd.grow_chunks);
        return;
    }

    memset(manager->address + (manager->num_chunks_mapped - 1) * manager->_chunk_size, int('a'), manager->_chunk_size);
}

void do_map_next_til_exhaustion(CmdMapUntilExhausted cmd, MmapManager *manager)
{

    MmapManagerMapNextOptions opts{};
    opts.dont_grow_if_fully_mapped = true;
    opts.chunks_to_map_next = cmd.chunks_per_increment;
    int num_chunks_unmapped = int(manager->num_chunks_reserved - manager->num_chunks_mapped);
    int64_t increments = ceil_div(int64_t(num_chunks_unmapped), int64_t(cmd.chunks_per_increment));
    auto remaining_chunks = int64_t(num_chunks_unmapped);

    PLOGI.printf("Will map %d chunks, in %d steps", remaining_chunks, increments);

    while (remaining_chunks > 0) {
        opts.chunks_to_map_next = std::min(uint64_t(remaining_chunks), cmd.chunks_per_increment);

        auto err = mmapext_map_next_file_chunk(manager, opts);
        if (err.error_code != MMAPEXT_ERR_NONE) {
            PLOGE.printf("failed to map next chunk: error_code: %d, error_message: %s",
                         err.error_code,
                         err.error_message);
            return;
        }

        PLOGI.printf("remaining_chunks = %li", remaining_chunks);

        remaining_chunks -= opts.chunks_to_map_next;
    }

    PLOGI.printf(
        "mapped all chunks (chunks: %lu, size: %lu bytes)", manager->num_chunks_mapped, mmapext_mapped_size(manager));

    opts.chunks_to_map_next = 1;
    opts.dont_grow_if_fully_mapped = true;
    opts.extra_chunks_to_reserve_on_grow = 0;

    auto err = mmapext_map_next_file_chunk(manager, opts);
    if (err.error_code == MMAPEXT_ERR_FULLY_MAPPED) {
        PLOGI.printf("As expected, we don't extend the file after address space is full");
    } else {
        PLOGI.printf("reserved_size = %lu, mapped_size = %lu, reserved_chunks = %lu, mapped_chunks = %lu",
                     mmapext_reserved_size(manager),
                     mmapext_mapped_size(manager),
                     manager->num_chunks_reserved,
                     manager->num_chunks_mapped);
        PLOGF.printf("Should have received an ERR_FULLY_MAPPED, got code: %d", err.error_code);
    }
}

void do_command(ReplCommand cmd, MmapManager *manager)
{
    switch (cmd.index()) {
    case CmdMapNextChunk::index: {
        auto mapNext = std::get<CmdMapNextChunk>(cmd);
        do_map_next(mapNext, manager);
        break;
    }

    case CmdPrintNumChunksMapped::index:
        printf("mapped_chunks = %d\n", manager->num_chunks_mapped);
        break;

    case CmdMapUntilExhausted::index:
        do_map_next_til_exhaustion(std::get<CmdMapUntilExhausted>(cmd), manager);
        break;

    default:
        PLOGE.printf("unknown command with type index %d", cmd.index());
    }
}