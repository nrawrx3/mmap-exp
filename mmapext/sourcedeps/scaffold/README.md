## A set of utilities thrown into a cauldron.

* Foundation library by Bitsquid plays a big part which resides in the top
  level.

* A scanner for tokenizing input.

* Some more allocators.

* Some more data structures.

## Notes

All containers only store POD-ish objects. Not thread-safe. The containers in
`collection_types.h`

- `Array<T>`
- `Queue<T>`
- `Hash<T>`

are mostly the same as in the `foundation` library, with move semantics added
to them.

The `pod_hash.h` file contains a generalized implementation of `Hash<T>`,
where the key can be any POD type. Hash functions for different types are
defined (and should be defined) in `pod_hash_usuals.h`

The `open_hash.h` file contains an open-addressed hash-table, usually faster
than the chaining based ones (at the cost of more memory).

The memory allocators and global variables are in the files `memory.h`,
`memory.cpp`, `arena_allocator.h`, `arena_allocator.cpp`, `buddy_allocator.h`,
`temp_allocator.h`, `temp_allocator.cpp`.

File `scanner.h` and `scanner.cpp` contain a 'usual' lexer (for ASCII only).

And some other stuff.
