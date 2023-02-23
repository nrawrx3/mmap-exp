#pragma once

#include <inttypes.h>
#include <mmapext/defs.h>
#include <stdbool.h>

// Everything inside this extern "C" block can be pasted as a Go comment and
// consumed by cgo. Don't forget to also paste the #includes.
extern "C" {

// Error codes

#define MMAPEXT_ERR_NONE 0
#define MMAPEXT_ERR_UNKNOWN 1
#define MMAPEXT_ERR_FAILED_TO_REMAP 2
#define MMAPEXT_ERR_FAILED_TO_MMAP 3
#define MMAPEXT_ERR_FAILED_TO_STAT_FILE 4
#define MMAPEXT_ERR_FAILED_TO_OPEN_FILE 5
#define MMAPEXT_ERR_FAILED_TO_FTRUNCATE 6
#define MMAPEXT_ERR_FAILED_TO_UNMAP 7
#define MMAPEXT_ERR_FAILED_TO_CLOSE_FILE 8
#define MMAPEXT_ERR_FULLY_MAPPED 9
#define MMAPEXT_ERR_PAGE_SIZE_NON_MULTIPLE 10

// The page size is fixed for now. 8KB is a good "max" estimate.
#if !defined(MMAPEXT_PAGE_SIZE)
#    define MMAPEXT_PAGE_SIZE 8192
#endif

struct MMAPEXT_API MmapManagerCreateOptions {
    // Path to backing file. File will be created if it doesn't exist.
    const char *backing_file;

    // Initial address-space to be reserved
    uint64_t initial_reserved_size;

    // If true, if current file size (which can be 0) is greater than
    // initial_reserved_size, ignore initial_reserved_size and reserve file size
    // amount of address space instead.
    _Bool reserve_existing_file_size;
};

struct MMAPEXT_API MmapManager {
    uint8_t *address;
    uint32_t num_chunks_reserved;
    uint32_t num_chunks_mapped;

    uint64_t _chunk_size;
    char *filepath;
    int _fd;
    int error_code;
    const char *error_message;
};

struct MMAPEXT_API ErrorResult {
    int error_code;
    const char *error_message;
    int saved_errno;
};

// Create a new mmap manager
MMAPEXT_API struct MmapManager mmapext_create_manager(struct MmapManagerCreateOptions opts);

// Delete an mmap manager
MMAPEXT_API struct ErrorResult mmapext_delete_manager(struct MmapManager *man);

// Returns true if all available address space is mapped
static inline _Bool mmapext_full(const struct MmapManager *man)
{
    return man->num_chunks_reserved == man->num_chunks_mapped;
}

static inline _Bool mmapext_is_alive(const struct MmapManager *man) { return man->address != 0; }

static inline uint64_t mmapext_reserved_size(const struct MmapManager *man)
{
    return man->num_chunks_reserved * man->_chunk_size;
}

// Returns size of currently mapped area.
static inline uint64_t mmapext_mapped_size(const struct MmapManager *man)
{
    return man->num_chunks_mapped * man->_chunk_size;
}

struct MMAPEXT_API MmapManagerMapNextOptions {
    // If reserved address space is fully mapped, mmapext_map_next_file_chunk
    // will fail if this is set to true.
    _Bool dont_grow_if_fully_mapped;

    // If all reserved address space is full, optionally grow the file by this
    // many chunks and remap the address space to this grown file. The new
    // reserved size, on success will be current num_chunks_reserved +
    // extra_chunks_to_reserve_on_grow.
    //
    // If set to 0, the file will not be grown and if address space is fully
    // mapped, mmapext_map_next_file_chunk will fail.
    uint64_t extra_chunks_to_reserve_on_grow;

    // When the file is grown, this many more chunks should be added to the
    // mapped address space. Should be equal or less than
    // chunks_to_reserve_on_grow. Can be 0, in which case the mapped
    // address-space is not extended to the grown file-address space.
    uint64_t chunks_to_map_next;
};

struct MMAPEXT_API MmapManagerMapNextChunkResult {
    struct ErrorResult error;

    // Mapped address space was moved as a result of map next chunk request. If
    // dont_grow_if_fully_mapped option was false, this is also going to be
    // false obviously.
    _Bool mapping_was_moved;

    // How much was the file extended, in bytes, as a result of map next chunk
    // request.
    uint64_t file_extension_size;
};

MMAPEXT_API struct MmapManagerMapNextChunkResult
mmapext_map_next_file_chunk(struct MmapManager *man, struct MmapManagerMapNextOptions opts);

// Map the full file. Will grow the reserved address space if not enough is
// already reserved. Call it right after
MMAPEXT_API struct MmapManagerMapNextChunkResult mmapext_map_full_file(struct MmapManager *man);

uint64_t mmapext_chunk_size();
} // extern "C"
