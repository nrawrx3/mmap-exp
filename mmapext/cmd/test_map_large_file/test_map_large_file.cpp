#include "argparse.hpp"
#include "util.hpp"

#include <linenoise.h>
#include <mmapext/mmapext.h>
#include <optional>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>
#include <scaffold/const_log.h>
#include <scaffold/scanner.h>
#include <sys/mman.h>
#include <variant>

using namespace std::string_literals;

std::string _format_memory_size(uint64_t size)
{
    auto quotients = std::array<uint64_t, 5>{ size, 0, 0, 0, 0 };
    auto rems = std::array<uint64_t, 5>{ size, 0, 0, 0, 0 };
    auto divisors = std::array<uint64_t, 5>{ 0, uint64_t(1) << 30, uint64_t(1) << 20, uint64_t(1) << 10, 1 };
    auto units = std::array<std::string, 5>{ "Total", "GB", "MB", "KB", "B" };

    for (auto i = 1; i < quotients.size(); i++) {
        rems[i] = rems[i - 1] % divisors[i];
        quotients[i] = rems[i - 1] / divisors[i];
    }

    std::string total;
    total.reserve(64);

    for (auto i = 1; i < quotients.size(); i++) {
        if (quotients[i] != 0) {
            total += std::to_string(quotients[i]);
            total += units[i];
            total += ";";
        }
    }
    return total;
}

struct Config {
    std::string filepath;
};

static Config config;

std::aligned_storage_t<sizeof(MmapManager), alignof(MmapManager)> _mmap_manager_store[1];

MmapManager *manager() { return reinterpret_cast<MmapManager *>(_mmap_manager_store); }

void must_create_file(std::string filepath, uint64_t initial_size)
{
    int fd = open(filepath.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        PLOGE.printf("failed to create file: %s", strerror(errno));
    }

    if (ftruncate(fd, initial_size) != 0) {
        PLOGE.printf("failed to ftruncate file %s: %s", filepath.c_str(), strerror(errno));
        exit(1);
    }

    if (close(fd) != 0) {
        PLOGE.printf("failed to close newly created backing file %s: %s", filepath.c_str(), strerror(errno));
        exit(0);
    }
}

void reset_file_size(std::string filepath)
{
    int fd = open(filepath.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        PLOGE.printf("failed to create file: %s", strerror(errno));
        exit(1);
    }

    if (ftruncate(fd, 0) != 0) {
        PLOGE.printf("failed to ftruncate file to 0 bytes %s: %s", filepath.c_str(), strerror(errno));
        exit(1);
    }
}

constexpr uint64_t GB = u64(1) << 30;
constexpr uint64_t MB = u64(1) << 20;
constexpr uint64_t KB = u64(1) << 10;

constexpr const char *backing_file = "test_backing_file";
constexpr auto initial_file_size = 4u * GB;
constexpr auto max_file_size = 20u * GB;
constexpr auto map_increment_size = 4u * GB;

constexpr auto chunks_per_increment = map_increment_size / MMAPEXT_PAGE_SIZE;
constexpr auto target_size = max_file_size;
constexpr auto target_mapped_chunks = target_size / MMAPEXT_PAGE_SIZE;

// Reserving full foreseeable file-size, so reserved address range doesn't need
// to move, just extended.
constexpr auto initial_reserved_size = max_file_size;

void map_large_file()
{
    must_create_file(backing_file, initial_file_size);
    // DEFERSTAT(reset_file_size(backing_file));

    auto create_opts = MmapManagerCreateOptions{
        .backing_file = backing_file,
        .initial_reserved_size = initial_reserved_size,
        .reserve_existing_file_size = false,
    };

    auto manager = mmapext_create_manager(create_opts);
    if (manager.error_code != MMAPEXT_ERR_NONE) {
        PLOGF.printf("failed to create manager: %s", manager.error_message);
    }
    DEFERSTAT(mmapext_delete_manager(&manager));

    PLOGI.printf("created manager with initial_file_size: %lu", initial_file_size);

    auto opts = MmapManagerMapNextOptions{};
    opts.dont_grow_if_fully_mapped = false; // We want the file to be grown.
    opts.chunks_to_map_next = map_increment_size / MMAPEXT_PAGE_SIZE;

    // const int64_t num_chunks_unmapped = int64_t(manager.num_chunks_reserved - manager.num_chunks_mapped);
    const int64_t num_chunks_unmapped =
        std::max(int64_t(0), int64_t(target_mapped_chunks - manager.num_chunks_mapped));
    const int64_t increments_needed = ceil_div(num_chunks_unmapped, int64_t(chunks_per_increment));

    PLOGI.printf("Will map %li chunks in %d increments, one increment = %li chunks",
                 num_chunks_unmapped,
                 increments_needed,
                 chunks_per_increment);

    PLOGI.printf("Pausing before mapping chunks...");
    getchar();

    for (int64_t remaining_chunks = num_chunks_unmapped; remaining_chunks > 0;
         remaining_chunks -= opts.chunks_to_map_next) {
        opts.chunks_to_map_next = std::min(remaining_chunks, int64_t(opts.chunks_to_map_next));
        auto res = mmapext_map_next_file_chunk(&manager, opts);
        if (res.error.error_code != MMAPEXT_ERR_NONE) {
            PLOGF.printf("failed to map next chunk: %s", res.error.error_message);
        }

        const auto cur_mapped_size = uint64_t(manager.num_chunks_mapped) * MMAPEXT_PAGE_SIZE;
        const auto cur_mapped_size_str = _format_memory_size(cur_mapped_size);

        PLOGI.printf(
            "Remaining chunks = %li, current mapped size: %s", remaining_chunks, cur_mapped_size_str.c_str());
        memset(manager.address, 170, cur_mapped_size); // 0b10101010
    }

    const auto cur_mapped_size = uint64_t(manager.num_chunks_mapped) * MMAPEXT_PAGE_SIZE;
    const auto cur_mapped_size_str = _format_memory_size(cur_mapped_size);

    PLOGI.printf("fully mapped targeted size: %s, filling with 1s", cur_mapped_size_str.c_str());
    // memset(manager.address, 1, cur_mapped_size);

    PLOGI.printf("fully mapped targeted size: %s, syncing...", cur_mapped_size_str.c_str());
    msync(manager.address, uint64_t(manager.num_chunks_mapped) * MMAPEXT_PAGE_SIZE, MS_SYNC);

    PLOGI.printf(
        "mapped all chunks: %lu, size: %lu bytes", manager.num_chunks_mapped, mmapext_mapped_size(&manager));
}

int main()
{
    fo::memory_globals::init();
    DEFERSTAT(fo::memory_globals::shutdown());

    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::verbose, &consoleAppender);

    auto pid = getpid();

    map_large_file();
}
