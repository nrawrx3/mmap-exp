## mmap-exp

(mmap experiments)

Wrapper around the mmap syscall and a cgo extension for Go.

- Allows reserving virtual address space to avoid invalidating pointers
- Allows append-only writing to memory backed by a file

The API works, but more polish required. In particular, write thin wrapper over
the syscalls and implement all the logic in Go instead of C.