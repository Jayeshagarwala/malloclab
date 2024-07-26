# Dynamic Storage Allocator Lab: Writing `malloc` in C

## Lab Description

In this lab, I wrote a dynamic storage allocator for C programs, implementing my own versions of the `malloc`, `free`, and `realloc` functions. This project involved creating a memory management system that is correct, space-efficient, and fast, with all modifications made in the `mm.c` file.

## Key Features

1. **Initialization Function (`mm_init`)**: Implemented the `mm_init` function to perform necessary initializations for the allocator, such as allocating the initial heap area. This function ensures that the heap is properly set up before any memory allocation requests are processed.

2. **Memory Allocation (`malloc`)**: Created a `malloc` function that returns a pointer to an allocated block of at least the requested size. Ensured that allocated blocks are within the heap region, do not overlap, and are 16-byte aligned.

3. **Memory Deallocation (`free`)**: Developed the `free` function to release memory blocks back to the allocator. Ensured that `free` only operates on pointers previously allocated by `malloc`, `calloc`, or `realloc` and that it correctly handles NULL pointers.

4. **Reallocation (`realloc`)**: Designed the `realloc` function to adjust the size of an existing memory block, returning a new pointer to the resized block. Managed the content transfer between old and new blocks while handling special cases such as NULL pointers and zero-size requests.

## Heap Consistency Checker

- **Heap Checker Implementation (`mm_checkheap`)**: Implemented a heap consistency checker to scan the heap and ensure its validity. The checker verifies multiple invariants, such as:
  - Ensuring all blocks in the free list are marked as free.
  - Checking for contiguous free blocks that should be coalesced.
  - Verifying that all free blocks are in the free list and that pointers in the free list are valid.
  - Ensuring no allocated blocks overlap.
  - Validating that pointers in heap blocks point to valid heap addresses.

By integrating the heap checker, I ensured robust debugging and validation of my memory allocator's correctness and efficiency.

Through this lab, I developed a deeper understanding of low-level memory management, pointer manipulation, and the intricacies of dynamic storage allocation in C.
