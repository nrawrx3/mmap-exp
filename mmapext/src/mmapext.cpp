#include <fcntl.h>
#include <mmapext/mmapext.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <array>
#include <optional>
#include <plog/Log.h>

constexpr uint64_t mmapext_page_size = MMAPEXT_PAGE_SIZE;
constexpr size_t safe_strerror_bufsize = 1024;

static std::string _format_memory_size(uint64_t size);
static std::optional<uint64_t> file_size(const char *filepath);

static ErrorResult _mmapext_map_next_chunk_dont_grow(struct MmapManager *man,
                                                     struct MmapManagerMapNextOptions opts);

static ErrorResult _mmapext_grow_reserved_address_space(MmapManager *man, uint64_t grow_num_chunks);

template <typename T> T align_forward(T value, T divisor)
{
    static_assert(std::is_integral<T>::value, "need T to be an integral");
    T mod = value % divisor;
    if (mod != T(0)) {
        return value + (divisor - mod);
    }
    return value;
}

template <size_t N> static char *safe_strerror(std::array<char, N> &arr, int cur_errno)
{
    std::fill(arr.begin(), arr.end(), 0);
    return strerror_r(cur_errno, arr.data(), arr.size());
}

std::optional<uint64_t> file_size(const char *filepath)
{
    // Unmap the current mapping and map an extended amount of file.
    struct stat statbuf;
    int err = stat(filepath, &statbuf);
    if (err != 0) {
        return std::optional<uint64_t>{};
    }
    return std::optional<uint64_t>{ statbuf.st_size };
}

ErrorResult extend_file_size(int fd, uint64_t file_size)
{
    int r = ftruncate(fd, file_size);
    if (r != 0) {
        return ErrorResult{
            .error_code = MMAPEXT_ERR_FAILED_TO_FTRUNCATE,
            .error_message = "failed to ftruncate file to multiple",
            .saved_errno = errno,
        };
    }
    return ErrorResult{ .error_code = MMAPEXT_ERR_NONE };
}

struct MmapManager mmapext_create_manager(MmapManagerCreateOptions opts)
{
    std::array<char, 1024> errno_desc_buf{};

    MmapManager manager{};

    if (opts.initial_reserved_size < mmapext_page_size) {
        opts.initial_reserved_size = mmapext_page_size;
    }

    manager._fd = open(opts.backing_file, O_RDWR | O_CREAT, 0644);
    if (manager._fd == -1) {
        manager.error_code = MMAPEXT_ERR_FAILED_TO_OPEN_FILE;
        manager.error_message = "failed to open backing file";
        PLOGE.printf("failed to open backing file: %s", safe_strerror(errno_desc_buf, errno));
        return manager;
    }

    auto existing_file_size_opt = file_size(opts.backing_file);
    if (!existing_file_size_opt) {
        manager.error_code = MMAPEXT_ERR_FAILED_TO_STAT_FILE;
        manager.error_message = "failed to stat given file for knowing initial file size";

        return manager;
    }

    uint64_t existing_file_size = existing_file_size_opt.value();
    PLOGI.printf("Existing file size = %lu", existing_file_size);

    uint64_t new_file_size = align_forward(existing_file_size, mmapext_page_size);

    auto err = extend_file_size(manager._fd, new_file_size);
    if (err.error_code != MMAPEXT_ERR_NONE) {
        PLOGE.printf(
            "failed top extend file size to chunk-size multiple, file: %s, existing size: %zu, errno: %s",
            opts.backing_file,
            existing_file_size,
            safe_strerror(errno_desc_buf, err.saved_errno));
        manager.error_code = err.error_code;
        manager.error_message = err.error_message;
        return manager;
    }

    uint64_t reserved_size = opts.initial_reserved_size;
    if (new_file_size > opts.initial_reserved_size && opts.reserve_existing_file_size) {
        reserved_size = new_file_size;
    }

    PLOGI.printf("inital reserved_size = %lu", reserved_size);

    manager.filepath = reinterpret_cast<char *>(malloc(strlen(opts.backing_file) + 1));
    strcpy(manager.filepath, opts.backing_file);

    void *addr = mmap(nullptr, reserved_size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (addr == nullptr) {
        manager.error_code = MMAPEXT_ERR_FAILED_TO_MMAP;
        manager.error_message = "failed to reserve initial address space with mmap";
        return manager;
    }

    manager.address = reinterpret_cast<uint8_t *>(addr);
    manager.num_chunks_reserved = reserved_size / mmapext_page_size;
    manager._chunk_size = mmapext_page_size;
    manager.num_chunks_mapped = 0;

    auto reserved_size_str = _format_memory_size(mmapext_reserved_size(&manager));

    PLOGI.printf("created manager with address space: %0x and size: %s (= %lu)",
                 manager.address,
                 reserved_size_str.c_str(),
                 mmapext_reserved_size(&manager));

    return manager;
}

ErrorResult mmapext_delete_manager(struct MmapManager *man)
{
    if (man == nullptr) {
        PLOGE.printf("unexpected nullptr");
        return ErrorResult{ .error_code = MMAPEXT_ERR_NONE };
    }

    int r = munmap(man->address, mmapext_reserved_size(man));
    if (r != 0) {
        PLOGE.printf("failed to unmap reserved address space");
        return ErrorResult{
            .error_code = MMAPEXT_ERR_FAILED_TO_UNMAP,
            .error_message = "failed to unmap reserved address space",
            .saved_errno = errno,
        };
    }
    PLOGI.printf("deleted manager with address space %lu", man->address);

    man->address = 0;
    if (man->_fd != -1) {
        r = close(man->_fd);
        if (r != 0) {
            return ErrorResult{
                .error_code = MMAPEXT_ERR_FAILED_TO_CLOSE_FILE,
                .error_message = "failed to close file after unmapping",
                .saved_errno = errno,
            };
        }
    }

    free(man->filepath);
    man->filepath = nullptr;

    return ErrorResult{ .error_code = MMAPEXT_ERR_NONE };
}

struct MmapManagerMapNextChunkResult mmapext_map_full_file(struct MmapManager *man)
{
    auto res = MmapManagerMapNextChunkResult{};
    auto fs = file_size(man->filepath);

    if (!fs) {
        res.error = ErrorResult{
            .error_code = MMAPEXT_ERR_FAILED_TO_STAT_FILE,
            .error_message = "failed to obtain file size",
            .saved_errno = errno,
        };
        return res;
    }

    if (fs.value() > mmapext_mapped_size(man)) {
        const auto remaining_size = fs.value() - mmapext_mapped_size(man);
        if (remaining_size % mmapext_page_size != 0) {
            res.error = ErrorResult{
                .error_code = MMAPEXT_ERR_PAGE_SIZE_NON_MULTIPLE,
                .error_message = "unmapped tail of file is not a multiple of page size",
            };
            return res;
        }

        auto opts = MmapManagerMapNextOptions{
            .dont_grow_if_fully_mapped = false,
            .extra_chunks_to_reserve_on_grow = 0,
            .chunks_to_map_next = remaining_size / mmapext_page_size,
        };

        return mmapext_map_next_file_chunk(man, opts);
    }
    return res;
}

struct MmapManagerMapNextChunkResult mmapext_map_next_file_chunk(struct MmapManager *man,
                                                                 struct MmapManagerMapNextOptions opts)
{
    std::array<char, safe_strerror_bufsize> errno_desc_buf{};

    // Unmap the current mapping and map an extended amount of file.
    struct stat statbuf;
    if (stat(man->filepath, &statbuf) != 0) {
        PLOGE.printf("failed to stat backing file: %s, errno = %d, error = %s",
                     man->filepath,
                     errno,
                     safe_strerror(errno_desc_buf, errno));

        auto err = ErrorResult{
            .error_code = MMAPEXT_ERR_FAILED_TO_STAT_FILE,
            .error_message = "failed to stat the managed backing file",
            .saved_errno = errno,
        };

        return MmapManagerMapNextChunkResult{ .error = err };
    }

    const uint64_t wanted_mapped_chunks = man->num_chunks_mapped + opts.chunks_to_map_next;

    bool need_to_grow_reserved_space = false;
    bool need_to_grow_file = false;
    uint64_t file_size_increment = 0;

    if (man->num_chunks_reserved >= wanted_mapped_chunks) {
        if (statbuf.st_size < wanted_mapped_chunks * man->_chunk_size) {
            // return _mmapext_map_next_chunk_dont_grow(man, opts);
            need_to_grow_file = true;
        }
    } else {
        need_to_grow_reserved_space = true;
    }

    PLOGI.printf("need to grow file and/or reserved address space: grow file? %d, grow reserved? %d",
                 need_to_grow_file,
                 need_to_grow_reserved_space);

    if (need_to_grow_file || need_to_grow_reserved_space) {
        if (opts.dont_grow_if_fully_mapped) {

            auto err = ErrorResult{
                .error_code = MMAPEXT_ERR_FULLY_MAPPED,
                .error_message = "address space fully mapped and opts.dont_grow_if_fully_mapped is True",
                .saved_errno = 0,
            };

            return MmapManagerMapNextChunkResult{ .error = err };
        }
    }

    if (need_to_grow_file) {
        const uint64_t new_file_size = wanted_mapped_chunks * man->_chunk_size;
        file_size_increment = new_file_size - uint64_t(statbuf.st_size);

        if (ftruncate(man->_fd, new_file_size) != 0) {
            PLOGE.printf("failed to extend file %s using ftruncate: %s",
                         man->filepath,
                         safe_strerror(errno_desc_buf, errno));
            auto err = ErrorResult{
                .error_code = MMAPEXT_ERR_FAILED_TO_REMAP,
                .error_message = "failed to extend file using ftruncate",
                .saved_errno = errno,
            };

            return MmapManagerMapNextChunkResult{ .error = err };
        }

        auto old_reserved_size_str = _format_memory_size(statbuf.st_size);
        auto new_reserved_size_str = _format_memory_size(new_file_size);

        PLOGI.printf("extended file with ftruncate from %s to %s",
                     old_reserved_size_str.c_str(),
                     new_reserved_size_str.c_str());
    }

    if (need_to_grow_reserved_space) {
        const auto reserve_grow_chunks =
            std::max(opts.extra_chunks_to_reserve_on_grow, opts.chunks_to_map_next);
        ErrorResult err = _mmapext_grow_reserved_address_space(man, reserve_grow_chunks);
        if (err.error_code != MMAPEXT_ERR_NONE) {
            PLOGE.printf("failed to grow reserved address space: %s", err.error_message);
            return MmapManagerMapNextChunkResult{ .error = err };
        }
        PLOGI.printf("grew reserved address space");

        // In this case we have to remap the address space. Map all the file chunks from start to desired.
        man->num_chunks_mapped = wanted_mapped_chunks;
        PLOGI.printf("Reallocated %zu chunks with %zu in total to be mapped (adding %zu chunks to currently "
                     "mapped chunks)",
                     man->num_chunks_reserved,
                     man->num_chunks_mapped,
                     opts.chunks_to_map_next);
        void *same_addr = mmap(man->address,
                               man->num_chunks_mapped * man->_chunk_size,
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED | MAP_FIXED,
                               man->_fd,
                               0);
        if (same_addr == nullptr) {
            auto reserved_size_str = _format_memory_size(mmapext_reserved_size(man));
            PLOGE.printf("failed to remap the file after extending address space to size %s, errno: %s",
                         reserved_size_str.c_str(),
                         safe_strerror(errno_desc_buf, errno));

            auto err = ErrorResult{
                .error_code = MMAPEXT_ERR_FAILED_TO_MMAP,
                .error_message =
                    R"(failed to mmap extended number of chunks after extending file and address space respectively.)",
                .saved_errno = errno,
            };
            return MmapManagerMapNextChunkResult{ .error = err };
        }

        return MmapManagerMapNextChunkResult{
            .error = ErrorResult{ .error_code = MMAPEXT_ERR_NONE },
            .mapping_was_moved = true,
            .file_extension_size = file_size_increment,
        };
    } else {
        auto err = _mmapext_map_next_chunk_dont_grow(man, opts);
        auto res = MmapManagerMapNextChunkResult{ .error = err };
        if (err.error_code != MMAPEXT_ERR_NONE) {
            return res;
        }
        res.file_extension_size = file_size_increment;
        res.mapping_was_moved = false;
        return res;
    }
}

ErrorResult _mmapext_map_next_chunk_dont_grow(struct MmapManager *man, struct MmapManagerMapNextOptions opts)
{
    // File, and therefore reserved address space is large enough to extend the file-mapping.
    uint64_t cur_mapped_size = man->num_chunks_mapped * man->_chunk_size;
    uint8_t *next_mapped_chunk_addr = man->address + cur_mapped_size;
    uint64_t next_mapped_chunk_size = opts.chunks_to_map_next * man->_chunk_size;
    
    void *mapped_addr = mmap(next_mapped_chunk_addr,
                             next_mapped_chunk_size,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_FIXED,
                             man->_fd,
                             cur_mapped_size);

    if (mapped_addr == nullptr) {
        PLOGE.printf("failed to extend mapping to existing file chunks from %d to %d chunks",
                     man->num_chunks_mapped,
                     man->num_chunks_mapped + opts.chunks_to_map_next);
        return ErrorResult{
            .error_code = MMAPEXT_ERR_FAILED_TO_REMAP,
            .error_message = "failed to remap (extend) current mapping within already reserved address space",
            .saved_errno = errno,
        };
    }

    PLOGI.printf("mapped %li chunks at tail", int64_t(opts.chunks_to_map_next));

    man->num_chunks_mapped += opts.chunks_to_map_next;
    return ErrorResult{};
}

ErrorResult _mmapext_grow_reserved_address_space(MmapManager *man, uint64_t grow_num_chunks)
{
    uint64_t new_reserved_size = (man->num_chunks_reserved + grow_num_chunks) * man->_chunk_size;

    // First we relinquish current reserved address space and then we map the
    // already-mapped address-space worth of memory plus the requested chunks to
    // map.
    int r = munmap(man->address, mmapext_reserved_size(man));
    if (r != 0) {
        return ErrorResult{
            .error_code = MMAPEXT_ERR_FAILED_TO_UNMAP,
            .error_message = "munmap() failed to unmap currently reserved address space",
            .saved_errno = errno,
        };
    }

    void *new_addr = mmap(nullptr, new_reserved_size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (new_addr == nullptr) {
        PLOGE.printf("failed to mmap after munmap in grow_reserved_address_space");

        return ErrorResult{
            .error_code = MMAPEXT_ERR_FAILED_TO_MMAP,
            .error_message = "failed to mmap after relinquishing old mapping",
            .saved_errno = errno,
        };
    }

    auto old_reserved_size_str = _format_memory_size(mmapext_reserved_size(man));
    auto new_reserved_size_str = _format_memory_size(new_reserved_size);

    PLOGI.printf("grew reserved address space from %s to %s",
                 old_reserved_size_str.c_str(),
                 new_reserved_size_str.c_str());

    man->address = reinterpret_cast<uint8_t *>(new_addr);
    man->num_chunks_reserved += grow_num_chunks;
    return ErrorResult{ .error_code = MMAPEXT_ERR_NONE };
}

uint64_t mmapext_chunk_size() { return mmapext_page_size; }

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
