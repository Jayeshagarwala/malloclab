/*
 * mm.c
 *
 * Name: Jayesh Agarwala
 *
 * DESIGN: 
 * 
 * Malloc implementation using segregated free lists and first fit allocation strategy. 
 * 
 * Segregated list contains explicit free list of 14 lists, 
 * each containing free blocks of size 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 and greater than 65536 bytes.
 * The free list is a circular doubly linked list with a head pointer.
 * 
 * Each free block contains a header and a footer, each of size 8 bytes.
 * The header contains the size of the block, the current allocated bit and the previous allocated bit.
 * The footer contains the size of the block and the current allocated bit.
 * 
 * The prologue (16 bytes) and epilogue (8 bytes) blocks are used to mark the start and end of the heap.
 * Padding (8 bytes) is used to ensure that the payload is 16-byte aligned.
 * 
 * The malloc function uses the best fit strategy to find a free block of sufficient size.
 * 
 * 
 * GLOBAL VARIABLE SPACE (128 bytes):
 * 
 * Prologue pointer: 8 bytes
 * Epilogue pointer: 8 bytes
 * Free list: 14 * 8 bytes = 112 bytes
 * 
 * 
 * HEAP CHECKER:
 * 
 * Useful tool for debugging and checking the correctness of the heap.
 * 
 * The heap checker checks the following invariants:
 * 1. Every block is 16-byte aligned
 * 2. Contiguous free blocks escaped coalescing
 * 3. The header and footer of each free block match
 * 4. No block exceeds heap size
 * 5. No block is outside the heap
 * 6. No allocated block is in the free list
 * 7. The next block pointer in consistent
 * 8. The prev block pointer in consistent
 * 9. The free block is in correct free list
 * 
 *
 * REFERENCES:
 * 1. Computer System's A Programmer's Perspective by Randal E. Bryant and David R. O'Hallaron (Chapter 9.9)
 * 2. CMPSC 473 Lecture Slides: Dynamic Memory Allocation by Timothy Zhu (Penn State University) 
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
//#define DEBUG

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
uint64_t* epilogue_ptr;

// rounds up to the nearest multiple of ALIGNMENT
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*
 * structure of the free list which is a circular doubly linked list with head pointer
 */
typedef struct free_list_node {
    struct free_list_node* prev;
    struct free_list_node* next;
} free_list_node_t;

/*
 * structure of the free list head
 */
typedef struct free_list {
    free_list_node_t* head;
} free_list_t;

free_list_t free_list[14];

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
 * packHeader: packs a size, current allocated bit and previous allocated bit into a word
 */
static uint64_t packHeader(uint64_t size, uint64_t is_allocated, uint64_t is_prev_allocated) {

    return  (uint64_t)(size | is_prev_allocated << 1 | is_allocated);
}

/*
 * packFooter: packs a size and allocated bit into a word
 */
static uint64_t packFooter(uint64_t size, uint64_t is_allocated) {

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
 * get_is_prev_allocated: reads if the previous block is allocated or not from header
 */
static uint64_t get_is_prev_allocated(uint64_t *ptr) {

    return (read_block(ptr) & 0x2) >> 1;

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
 * get_list_index: returns the index of the free list, given block size
 */
static int get_list_index(uint64_t size) {

    int index = 0; 

    if(size < 17){
        index = 0;
    } 
    else if(size < 33) {
        index = 1;
    } 
    else if(size < 65) {
        index = 2;
    } 
    else if(size < 129) {
        index = 3;
    } 
    else if(size < 257) {
        index = 4;
    } 
    else if(size < 513) {
        index = 5;
    } 
    else if(size < 1025) {
        index = 6;
    } 
    else if(size < 2049) {
        index = 7;
    } 
    else if(size < 4097) {
        index = 8;
    } 
    else if(size < 8193) {
        index = 9;
    } 
    else if(size < 16385) {
        index = 10;
    } 
    else if(size < 32769) {
        index = 11;
    } 
    else if(size < 65537) {
        index = 12;
    } 
    else {
        index = 13;
    }

    return index;

}

/*
 * insert_free_block: inserts a free block into the free list
 */
static void __attribute__ ((noinline)) insert_free_block(free_list_node_t* free_block, int index) {

    //if the free list is empty
    if (free_list[index].head == NULL) {
        free_list[index].head = free_block;
        free_block->next = free_block;
        free_block->prev = free_block;
    } 
    // if the free list is not empty
    else {
        free_block->next = free_list[index].head;
        free_block->prev = free_list[index].head->prev;
        free_list[index].head->prev->next = free_block;
        free_list[index].head->prev = free_block;
    }

}

/*
 * remove_free_block: removes a free block from the free list
 */
static void __attribute__ ((noinline)) remove_free_block(free_list_node_t* free_block, int index) {

    // If the free block is the head of the free list
    if (free_block == free_list[index].head) {
        // if free block is the only block in the free list
        if (free_block->next == free_block) {
            free_list[index].head->next = NULL;
            free_list[index].head->prev = NULL;
            free_list[index].head = NULL;
            return;
        } else {
            free_list[index].head = free_block->next;
        }
    }
    // if the free block is not the head of the free list
    free_block->prev->next = free_block->next;
    free_block->next->prev = free_block->prev;
    free_block->next = NULL;
    free_block->prev = NULL;

    return; 

}

/*
 * coalesce: coalesces the current free block with the adjacent free block and returns the new block head pointer
 */
static uint64_t* coalesce(uint64_t *ptr){

    uint64_t block_size = get_block_size(ptr);
    uint64_t* next_block = get_next_block(ptr);
    uint64_t is_previous_allocated = get_is_prev_allocated(ptr);
    uint64_t is_next_allocated = get_is_allocated(next_block);

    // if the previous and next blocks are allocated, return the current block
    if(is_previous_allocated == 1 && is_next_allocated == 1){
        return ptr;
    }
    // if the previous block is free, remove it from the free list and coalesce
    else if(is_previous_allocated == 0 && is_next_allocated == 1){
        uint64_t* prev_block = get_prev_block(ptr);
        remove_free_block((free_list_node_t*)get_block_payload(prev_block), get_list_index(get_block_size(prev_block)));
        remove_free_block((free_list_node_t*)get_block_payload(ptr), get_list_index(get_block_size(ptr)));

        block_size += get_block_size(prev_block);
        write_block(prev_block, packHeader(block_size, 0, 1));
        write_block(get_footer(ptr), packFooter(block_size, 0));
        
        ptr = prev_block;

    }
    // if the next block is free, remove it from the free list and coalesce
    else if(is_previous_allocated == 1 && is_next_allocated == 0){

        remove_free_block((free_list_node_t*)get_block_payload(next_block), get_list_index(get_block_size(next_block)));
        remove_free_block((free_list_node_t*)get_block_payload(ptr), get_list_index(get_block_size(ptr)));

        block_size += get_block_size(next_block);
        write_block(ptr, packHeader(block_size, 0, 1));
        write_block(get_footer(ptr), packFooter(block_size, 0));

    }
    // if both the previous and next blocks are free, remove them from the free list and coalesce
    else{
        uint64_t* prev_block = get_prev_block(ptr);
        remove_free_block((free_list_node_t*)get_block_payload(next_block), get_list_index(get_block_size(next_block)));
        remove_free_block((free_list_node_t*)get_block_payload(ptr), get_list_index(get_block_size(ptr)));
        remove_free_block((free_list_node_t*)get_block_payload(prev_block), get_list_index(get_block_size(prev_block)));

        block_size += get_block_size(prev_block) + get_block_size(next_block);
        write_block(get_prev_block(ptr), packHeader(block_size, 0, 1));
        write_block(get_footer(next_block), packFooter(block_size, 0));

        ptr = prev_block;

    }

    // insert the coalesced block into the free list
    free_list_node_t* new_free_block = (free_list_node_t*)get_block_payload(ptr);
    insert_free_block(new_free_block, get_list_index(block_size));
    write_block(get_block_payload(ptr),*((uint64_t *)new_free_block)); // set the payload to the free list node

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
    uint64_t is_prev_allocated = get_is_prev_allocated(new_block_ptr);
    
    write_block(new_block_ptr, packHeader(new_block_size, 0, is_prev_allocated)); // New block header
    write_block(get_footer(new_block_ptr), packFooter(new_block_size, 0)); // New block footer
    write_block(get_next_block(new_block_ptr), packHeader(0, 1, 0)); // New epilogue header
    epilogue_ptr = get_next_block(new_block_ptr);

    int index = get_list_index(new_block_size);

    free_list_node_t* new_free_block = (free_list_node_t*)get_block_payload(new_block_ptr);
    insert_free_block(new_free_block, index);
    write_block(get_block_payload(new_block_ptr), *((uint64_t *) new_free_block)); // set the payload to the free list node

    return coalesce(new_block_ptr);

}


/*
 * find_first_fit: finds the first fit free block of size size
 */
static uint64_t* find_first_fit(uint64_t size) {

    free_list_node_t *current_block_ptr;
    
    int index = get_list_index(size);
    
    for (int i = index; i < 14; i++) {
        if (free_list[i].head == NULL) {
            continue;
        }
        //search for a free block of sufficient size
        for (current_block_ptr = free_list[i].head; get_block_size(get_header((uint64_t *)current_block_ptr)) > 0; current_block_ptr = current_block_ptr->next) {
            if(get_block_size(get_header((uint64_t *)current_block_ptr)) >= size){
                remove_free_block(current_block_ptr, i);
                return get_header((uint64_t *)current_block_ptr);
            }
            else if(current_block_ptr == free_list[i].head->prev){
                break; 
            }
        }
    }
    
    return NULL;

}

/*
 * find_best_fit: finds the best fit free block of size size in an index, if not found, search first fit in the next index
 */
static uint64_t* find_best_fit(uint64_t size){
    free_list_node_t *current_block_ptr;

    free_list_node_t *best_free_block_ptr = NULL;
    uint64_t best_block_size = UINT64_MAX;
    int best_index;
    
    int index = get_list_index(size);

    for (int i = index; i < 14; i++) {

        if (free_list[i].head == NULL) {
            continue;
        }

        //search for a free block of best size
        current_block_ptr = free_list[i].head;
        do{
            if(get_block_size(get_header((uint64_t *)current_block_ptr)) == size){
                remove_free_block(current_block_ptr, i);
                return get_header((uint64_t *)current_block_ptr);
            }
            else if(get_block_size(get_header((uint64_t *)current_block_ptr)) > size && get_block_size(get_header((uint64_t *)current_block_ptr)) < best_block_size){
                best_free_block_ptr = current_block_ptr;
                best_block_size = get_block_size(get_header((uint64_t *)current_block_ptr));
                best_index = i;

                if (i != index){
                    remove_free_block(best_free_block_ptr, best_index);
                    return get_header((uint64_t *)best_free_block_ptr);
                }

            }

            current_block_ptr = current_block_ptr->next;

        } while (current_block_ptr != free_list[i].head);

        if(best_free_block_ptr != NULL && i == index){
            remove_free_block(best_free_block_ptr, best_index);
            return get_header((uint64_t *)best_free_block_ptr);
        }
        
    }
    
    return NULL;
}

/*
 * allocate_block: allocates a block of size size
 */
static void allocate_block(uint64_t *ptr, uint64_t size) {

    uint64_t block_size = get_block_size(ptr);

    if (block_size - size > (HEADER_SIZE + FOOTER_SIZE)) {

        //divided allocated block memory and remaining free memory

        write_block(ptr, packHeader(size, 1, get_is_prev_allocated(ptr))); // New allocated block header

        uint64_t* new_free_block = get_next_block(ptr);
        write_block(new_free_block, packHeader(block_size - size, 0, 1)); // New free block header
        write_block(get_footer(new_free_block), packFooter(block_size - size, 0)); // New free block footer

        uint64_t* next_block_ptr = get_next_block(new_free_block);
        uint64_t next_block_size = get_block_size(next_block_ptr);
        uint64_t is_next_allocated = get_is_allocated(next_block_ptr);

        write_block(next_block_ptr, packHeader(next_block_size, is_next_allocated, 0)); // update header of next block with previous allocated bit

        free_list_node_t* new_free_block_payload = (free_list_node_t*)get_block_payload(new_free_block);
        int index = get_list_index(block_size - size);
        insert_free_block(new_free_block_payload, index);
        write_block(get_block_payload(new_free_block), *((uint64_t *) new_free_block_payload)); // set the payload to the free list node

    } else {
        //allocated all the remaining memory to the allocated block

        write_block(ptr, packHeader(block_size, 1, get_is_prev_allocated(ptr))); // New allocated block header

        uint64_t* next_block_ptr = get_next_block(ptr);
        uint64_t next_block_size = get_block_size(next_block_ptr);
        uint64_t is_next_allocated = get_is_allocated(next_block_ptr);

        write_block(next_block_ptr, packHeader(next_block_size, is_next_allocated, 1)); // update header of next block with previous allocated bit

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
    
    for (int i = 0; i < 14; i++) {
        free_list[i].head = NULL;
    }

    if (prologue_ptr == (void *)-1)
        return false;

    write_block(prologue_ptr, 0); // Alignment padding
    write_block(prologue_ptr + (PADDING_SIZE/UINT64_T_SIZE), packHeader(PROLOGUE_SIZE, 1, 0)); // Prologue header
    write_block(prologue_ptr + ((PADDING_SIZE + HEADER_SIZE)/UINT64_T_SIZE), packFooter(PROLOGUE_SIZE, 1)); // Prologue footer
    write_block(prologue_ptr + ((PADDING_SIZE + PROLOGUE_SIZE)/UINT64_T_SIZE), packHeader(0, 1, 1)); // Epilogue header

    prologue_ptr += (PADDING_SIZE /UINT64_T_SIZE);

    epilogue_ptr = prologue_ptr + (PROLOGUE_SIZE/UINT64_T_SIZE);

    return true;
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    // IMPLEMENT THIS


    if (size < 1)
        return NULL;

    if (size < 16){
        size = 16;
    }

    uint64_t current_block_size = (uint64_t)align(size + HEADER_SIZE);
    uint64_t *free_block_ptr = find_first_fit(current_block_size);

    if (free_block_ptr != NULL){
        allocate_block(free_block_ptr, current_block_size);
        return get_block_payload(free_block_ptr);
    }

    //if no free block of sufficient size is found, expand the heap
    uint64_t *new_block_ptr = expand_heap(current_block_size);

    if (new_block_ptr == NULL)
        return NULL;

    remove_free_block((free_list_node_t*)get_block_payload(new_block_ptr), get_list_index(get_block_size(new_block_ptr)));
    allocate_block(new_block_ptr, current_block_size);
    return get_block_payload(new_block_ptr);

}

/*
 * free
 */
void free(void* ptr)
{
    // IMPLEMENT THIS

    if (ptr == NULL)
        return;

    uint64_t* header_ptr = get_header(ptr);
    uint64_t block_size = get_block_size(header_ptr);
    free_list_node_t* free_block = (free_list_node_t*)ptr;

    if (get_next_block(header_ptr) == epilogue_ptr) {
        write_block(epilogue_ptr, packHeader(0, 1, 0)); // update epilogue header with previous allocated bit
    }
    
    write_block(get_footer(header_ptr), packFooter(block_size, 0)); // new free block footer
    write_block(header_ptr, packHeader(block_size, 0, get_is_prev_allocated(header_ptr))); // new free block header

    uint64_t* next_block_ptr = get_next_block(header_ptr);
    uint64_t next_block_size = get_block_size(next_block_ptr);
    uint64_t is_next_allocated = get_is_allocated(next_block_ptr);

    write_block(next_block_ptr, packHeader(next_block_size, is_next_allocated, 0));

    insert_free_block(free_block, get_list_index(block_size));
    write_block(ptr,*((uint64_t *) free_block)); // set the payload to the free list node
    
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

    if(oldptr == NULL){
        return malloc(size);
    }

    if(size == 0){
        free(oldptr);
        return NULL;
    }

    uint64_t* old_block_ptr = get_header(oldptr);
    uint64_t old_block_size = get_block_size(old_block_ptr);

    //align the size
    if (size < 16){
        size = 16;
    }

    uint64_t new_block_size = (uint64_t)align(size + HEADER_SIZE);

    //if the new size is same as the old size, return the old pointer
    if(old_block_size == new_block_size){
        return oldptr;
    }
    //if the new size is less than the old size, allocate the block and return the old pointer
    else if(old_block_size > new_block_size){
        allocate_block(old_block_ptr, new_block_size);
        return oldptr;
    }
    //if the new size is greater than the old size, allocate a new block and copy the old block to the new block
    else{
        uint64_t* new_block_ptr = malloc(size);
        if(new_block_ptr == NULL){
            return NULL;
        }
        memcpy(new_block_ptr, oldptr, old_block_size - HEADER_SIZE);
        free(oldptr);
        return new_block_ptr;
    }
    
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

    uint64_t* current_block_ptr = prologue_ptr;

    while(current_block_ptr != epilogue_ptr){

        uint64_t current_block_size = get_block_size(current_block_ptr);

        //check if every block is 16-byte aligned
        if(current_block_size % 16 != 0){
            dbg_printf("Error: Block at %p is not 16-byte aligned\n", current_block_ptr);
        }
        
        //check if contiguous free blocks escaped coalescing
        if(get_is_allocated(current_block_ptr) == 0 && get_is_prev_allocated(current_block_ptr) == 0){
            dbg_printf("Error: Contiguous free blocks at %p and %p escaped coalescing\n", get_prev_block(current_block_ptr), current_block_ptr);
        }

        //check if the header and footer of each free block match
        //check for size bits
        if(get_is_allocated(current_block_ptr) == 0 && get_block_size(current_block_ptr) != get_block_size(get_footer(current_block_ptr))){
            dbg_printf("Error: Header and footer of free block at %p do not match in size bits\n", current_block_ptr);
        }
        //check for allocated bits
        if(get_is_allocated(current_block_ptr) == 0 && get_is_allocated(current_block_ptr) != get_is_allocated(get_footer(current_block_ptr))){
            dbg_printf("Error: Header and footer of free block at %p do not match in allocated bits\n", current_block_ptr);
        }
        
        //check if any block exceed heap size
        if(get_block_size(current_block_ptr) > mem_heapsize()){
            dbg_printf("Error: Block at %p exceeds heap size\n", current_block_ptr);
        }

        //check if any block is outside the heap
        if(!in_heap(current_block_ptr)){
            dbg_printf("Error: Block at %p is outside the heap\n", current_block_ptr);
        }

        current_block_ptr = get_next_block(current_block_ptr);


    }

    for(int i = 0; i<14 ;i++){

        if (free_list[i].head != NULL){
            free_list_node_t *current_block_ptr = free_list[i].head;
            do{
                //check if any allocated block is in the free list
                if(get_is_allocated(get_header((uint64_t *)current_block_ptr)) == 1){
                    dbg_printf("Error: Allocated block at %p is in the free list\n", get_header((uint64_t *)current_block_ptr));
                }

                //check if the next block pointer in consistent
                if(current_block_ptr->next != NULL){
                    //check if the next is pointing inside the heap
                    if(!in_heap(get_header((uint64_t *)current_block_ptr->next))){
                        dbg_printf("Error: Next pointer of block at %p is outside the heap\n", get_header((uint64_t *)current_block_ptr));
                    }
                    //check if the next block is pointing to a free block
                    if(get_is_allocated(get_header((uint64_t *)current_block_ptr->next)) == 1){
                        dbg_printf("Error: Next pointer of block at %p is pointing to an allocated block\n", get_header((uint64_t *)current_block_ptr));
                    }
                    //check if the next block is pointing to a block in the same free list
                    if(get_list_index(get_block_size(get_header((uint64_t *)current_block_ptr->next))) != i){
                        dbg_printf("Error: Next pointer of block at %p is pointing to a block in a different free list\n", get_header((uint64_t *)current_block_ptr));
                    }
                }

                //check if the prev block pointer in consistent
                if(current_block_ptr->prev != NULL){
                    //check if the prev is pointing inside the heap
                    if(!in_heap(get_header((uint64_t *)current_block_ptr->prev))){
                        dbg_printf("Error: Prev pointer of block at %p is outside the heap\n", get_header((uint64_t *)current_block_ptr));
                    }
                    //check if the prev block is pointing to a free block
                    if(get_is_allocated(get_header((uint64_t *)current_block_ptr->prev)) == 1){
                        dbg_printf("Error: Prev pointer of block at %p is pointing to an allocated block\n", get_header((uint64_t *)current_block_ptr));
                    }
                    //check if the prev block is pointing to a block in the same free list
                    if(get_list_index(get_block_size(get_header((uint64_t *)current_block_ptr->prev))) != i){
                        dbg_printf("Error: Prev pointer of block at %p is pointing to a block in a different free list\n", get_header((uint64_t *)current_block_ptr));
                    }
                }
                
                //check if the free block is in correct free list
                if(get_list_index(get_block_size(get_header((uint64_t *)current_block_ptr))) != i){
                    dbg_printf("Error: Free block at %p is in wrong free list\n", get_header((uint64_t *)current_block_ptr));
                }

                current_block_ptr = current_block_ptr->next;
                
            } while(current_block_ptr != free_list[i].head);
            
            
        }
    }

#endif // DEBUG
    return true;
}
