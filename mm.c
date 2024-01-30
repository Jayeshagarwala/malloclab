/*
 * mm.c
 *
 * Name: [FILL IN]
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * Also, read the README carefully and in its entirety before beginning.
 *
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

#ifdef DEBUG
// When debugging is enabled, the underlying functions get called
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
// When debugging is disabled, no code gets generated
#define dbg_printf(...)
#define dbg_assert(...)
#endif // DEBUG

// do not change the following!
#ifdef DRIVER
// create aliases for driver tests
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mm_memset
#define memcpy mm_memcpy
#endif // DRIVER

#define ALIGNMENT 16

#define PADDING_SIZE 8
#define HEADER_SIZE 8
#define FOOTER_SIZE 8
#define PROLOGUE_SIZE 16
#define EPILOGUE_SIZE 8

void* prologue_ptr; 
void* heap_list_ptr;

// rounds up to the nearest multiple of ALIGNMENT
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*
 * pack: packs a size and allocated bit into a word
 */

static size_t pack(size_t size, bool is_allocated) {

    return size | is_allocated;
}

/*
 * get_block_size: reads the block size from header or footer
 */
static size_t get_block_size(void *ptr) {

    return read_block(ptr) & ~0x7;

}

/*
 * get_is_allocated: reads if the block is allocated or not from header or footer
 */

static bool get_is_allocated(void *ptr) {

    return read_block(ptr) & 0x1;

}

/*
 * read_block: reads a word at address ptr
 */

static size_t read_block(void *ptr) {

    return *((size_t *)ptr);

}

/*
 * write_block: writes a word at address ptr
 */

static void write_block(void *ptr, size_t val) {

    *((size_t *)ptr) = val;

}

/*
 * get_header: returns the header address of a block, given block pointer
 */

static void *get_header(void *ptr) {

    return (void *)((size_t *)ptr - HEADER_SIZE);

}

/*
 * get_footer: returns the footer address of a block, given block pointer
 */

static void *get_footer(void *ptr) {

    return (void *)((size_t *)ptr + get_block_size(get_header(ptr)) - FOOTER_SIZE);

}

/*
 * get_next_block: returns the next block address, given block pointer
 */
static void *get_next_block(void *ptr) {

    return (void *)((size_t *)ptr + get_block_size(get_header(ptr)));

}

/*
 * get_prev_block: returns the previous block address, given block pointer
 */
static void *get_prev_block(void *ptr) {

    return (void *)((char *)ptr - get_block_size((void *)((char *)ptr - HEADER_SIZE - FOOTER_SIZE)));

}

/*
 * mm_init: returns false on error, true on success.
 */
bool mm_init(void)
{
    // IMPLEMENT THIS

    /* Create the initial empty heap */
    heap_list_ptr = mem_sbrk(PADDING_SIZE + PROLOGUE_SIZE + EPILOGUE_SIZE);

    if (heap_list_ptr == (void *)-1)
        return false;

    prologue_ptr = heap_list_ptr + PADDING_SIZE;

    write_block(heap_list_ptr, 0); // Alignment padding
    write_block(heap_list_ptr + PADDING_SIZE, pack(PROLOGUE_SIZE, true)); // Prologue header
    write_block(heap_list_ptr + PADDING_SIZE + HEADER_SIZE, pack(PROLOGUE_SIZE, true)); // Prologue footer
    write_block(heap_list_ptr + PADDING_SIZE + PROLOGUE_SIZE, pack(EPILOGUE_SIZE, true)); // Epilogue header

    heap_list_ptr += PADDING_SIZE + EPILOGUE_SIZE;

    return true;
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    // IMPLEMENT THIS
    return NULL;
}

/*
 * free
 */
void free(void* ptr)
{
    // IMPLEMENT THIS
    return;
}

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    // IMPLEMENT THIS
    return NULL;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mm_heap_hi() && p >= mm_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 * You call the function via mm_checkheap(__LINE__)
 * The line number can be used to print the line number of the calling
 * function where there was an invalid heap.
 */
bool mm_checkheap(int line_number)
{
#ifdef DEBUG
    // Write code to check heap invariants here
    // IMPLEMENT THIS
#endif // DEBUG
    return true;
}
