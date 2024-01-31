/*
 * mm.c
 *
 * Name: Jayesh Agarwala
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
#define UINT64_T_SIZE 8 

uint64_t* prologue_ptr; 

// rounds up to the nearest multiple of ALIGNMENT
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*
 * read_block: reads a word at address ptr
 */

static uint64_t read_block(uint64_t *ptr) {

    return *((uint64_t *)ptr);

}

/*
 * write_block: writes a word at address ptr
 */

static void write_block(uint64_t *ptr, uint64_t val) {

    *((uint64_t *)ptr) = val;

}


/*
 * pack: packs a size and allocated bit into a word
 */

static uint64_t pack(uint64_t size, uint64_t is_allocated) {

    return  (uint64_t)(size | is_allocated);
}

/*
 * get_block_size: reads the block size from header or footer
 */
static uint64_t get_block_size(uint64_t *ptr) {

    return read_block(ptr) & ~0x7;

}

/*
 * get_is_allocated: reads if the block is allocated or not from header or footer
 */

static uint64_t get_is_allocated(uint64_t *ptr) {

    return read_block(ptr) & 0x1;

}

/*
 * get_header: returns the header address of a block, given block payload pointer
 */

static uint64_t* get_header(uint64_t *ptr) {

    return (uint64_t *)((uint64_t *)ptr - (HEADER_SIZE/UINT64_T_SIZE));

}

/*
 * get_footer: returns the footer address of a block, given block header pointer
 */

static uint64_t* get_footer(uint64_t *ptr) {

    return (uint64_t *)((uint64_t *)ptr + ((get_block_size(ptr) - FOOTER_SIZE)/UINT64_T_SIZE));

}

/*
 * get_next_block: returns the next block address, given block pointer
 */
static uint64_t* get_next_block(uint64_t *ptr) {

    return (uint64_t *)((uint64_t *)ptr + (get_block_size(ptr)/UINT64_T_SIZE));

}

/*
 * get_prev_block: returns the previous block address, given block pointer
 */
static uint64_t* get_prev_block(uint64_t *ptr) {

    return ptr - (get_block_size(ptr - ((FOOTER_SIZE)/UINT64_T_SIZE))/UINT64_T_SIZE);

}

/*
 * get_block_payload: returns the payload address, given block pointer
 */

static uint64_t* get_block_payload(uint64_t *ptr) {

    return (uint64_t *)((uint64_t *)ptr + (HEADER_SIZE/UINT64_T_SIZE));

}

/*
 * coalesce: coalesces the current block with the adjacent block if it is free
 */
static uint64_t* coalesce(uint64_t *ptr){

    uint64_t block_size = get_block_size(ptr);
    uint64_t is_previous_allocated = get_is_allocated(get_prev_block(ptr));
    uint64_t is_next_allocated = get_is_allocated(get_next_block(ptr));

    if(is_previous_allocated == 1 && is_next_allocated == 1){
        return ptr;
    }
    else if(is_previous_allocated == 0 && is_next_allocated == 1){
        block_size += get_block_size(get_prev_block(ptr));
        write_block(get_prev_block(ptr), pack(block_size, 0));
        write_block(get_footer(ptr), pack(block_size, 0));
    }
    else if(is_previous_allocated == 1 && is_next_allocated == 0){
        block_size += get_block_size(get_next_block(ptr));
        write_block(ptr, pack(block_size, 0));
        write_block(get_footer(ptr), pack(block_size, 0));
        ptr = get_prev_block(ptr);
    }
    else{
        block_size += get_block_size(get_prev_block(ptr)) + get_block_size(get_next_block(ptr));
        write_block(get_prev_block(ptr), pack(block_size, 0));
        write_block(get_footer(get_next_block(ptr)), pack(block_size, 0));
        ptr = get_prev_block(ptr);
    }

    return ptr; 
}

/*
 * expand_heap: expands the heap by new_block_size bytes and returns the pointer to the new block
 */

static uint64_t* expand_heap(uint64_t new_block_size)
{

    uint64_t* new_block_ptr = (uint64_t *)mem_sbrk(new_block_size);

    if (new_block_ptr == (void *)-1)
        return NULL;
    
    new_block_ptr -= (HEADER_SIZE/UINT64_T_SIZE); // New block header

    write_block(new_block_ptr, pack(new_block_size, 0)); // New block header
    write_block(get_footer(new_block_ptr), pack(new_block_size, 0)); // New block footer
    write_block(get_next_block(new_block_ptr), pack(0, 1)); // New epilogue header

    return coalesce(new_block_ptr);

}
/*
 * allocate_block: allocates a block of size size
 */

static void allocate_block(uint64_t *ptr, uint64_t size) {

    uint64_t block_size = get_block_size(ptr);

    if (block_size - size >= (HEADER_SIZE + FOOTER_SIZE)) {

        //divided allocated block memory and remaining free memory

        write_block(ptr, pack(size, 1)); // New allocated block header
        write_block(get_footer(ptr), pack(size, 1)); // New allocated block footer
        write_block(get_next_block(ptr), pack(block_size - size, 0)); // New free block header
        write_block(get_footer(get_next_block(ptr)), pack(block_size - size, 0)); // New free block footer

    } else {
        //allocated all the remaining memory to the allocated block

        write_block(ptr, pack(block_size, 1)); // New allocated block header
        write_block(get_footer(ptr), pack(block_size, 1)); // New allocated block footer

    }

}


/*
 * mm_init: returns false on error, true on success.
 */
bool mm_init(void)
{
    // IMPLEMENT THIS

    //Create the initial empty heap
    prologue_ptr = (uint64_t *)mem_sbrk(PADDING_SIZE + PROLOGUE_SIZE + EPILOGUE_SIZE);

    if (prologue_ptr == (void *)-1)
        return false;

    write_block(prologue_ptr, 0); // Alignment padding
    write_block(prologue_ptr + (PADDING_SIZE/UINT64_T_SIZE), pack(PROLOGUE_SIZE, 1)); // Prologue header
    write_block(prologue_ptr + ((PADDING_SIZE + HEADER_SIZE)/UINT64_T_SIZE), pack(PROLOGUE_SIZE, 1)); // Prologue footer
    write_block(prologue_ptr + ((PADDING_SIZE + PROLOGUE_SIZE)/UINT64_T_SIZE), pack(0, 1)); // Epilogue header

    prologue_ptr += (PADDING_SIZE /UINT64_T_SIZE);

    return true;
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    // IMPLEMENT THIS

    uint64_t current_block_size = (uint64_t)align(size + HEADER_SIZE + FOOTER_SIZE);
    uint64_t *current_block_ptr;

    //search for a free block of sufficient size
    for (current_block_ptr = prologue_ptr; get_block_size(current_block_ptr) > 0; current_block_ptr = get_next_block(current_block_ptr)){
        if(get_is_allocated(current_block_ptr) == 0 && get_block_size(current_block_ptr) >= current_block_size){
            allocate_block(current_block_ptr, current_block_size);
            return get_block_payload(current_block_ptr);
        }
    }

    //if no free block of sufficient size is found, expand the heap
    uint64_t *new_block_ptr = expand_heap(current_block_size);

    if (new_block_ptr == NULL)
        return NULL;

    allocate_block(new_block_ptr, current_block_size);
    return get_block_payload(new_block_ptr);

}

/*
 * free
 */
void free(void* ptr)
{
    // IMPLEMENT THIS
    uint64_t* header_ptr = get_header(ptr);
    uint64_t block_size = get_block_size(header_ptr);

    write_block(get_footer(header_ptr), pack(block_size, 0)); // new free block footer
    write_block(header_ptr, pack(block_size, 0)); // new free block header
    
    //coalesce if possible
    coalesce(header_ptr);

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
