/*
 * File: allocator.c
 * Author: Qiujiang Jin 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "allocator.h"
#include "segment.h"

// Heap blocks are required to be aligned to 8-byte boundary
#define ALIGNMENT 8
// Size of the head and the foot
#define HFSIZE sizeof(size_t)
// Size of the pointer 
#define PTRSIZE sizeof(void *)

static void *base = NULL;

static void *end = NULL;

static void *free_list_head = NULL;

static inline void write_size(void *ptr, size_t sz, size_t alloc) {
    *(size_t *)ptr = sz | alloc;
}

static inline size_t read_size(void *ptr) {
    return *(size_t *)ptr & ~0x7;
}

static inline size_t read_alloc(void *ptr) {
    return *(size_t *)ptr & 0x1;
}

// Very efficient bitwise round of sz up to nearest multiple of mult
static inline size_t roundup(size_t sz, size_t mult) {
    return (sz + mult-1) & ~(mult-1);
}

static inline void *payload_to_head(void *ptr) {
    return (char *)ptr - HFSIZE;
}

static inline void *head_to_payload(void *ptr) {
    return (char *)ptr + HFSIZE;
}

// Transform from head to the pointer pointing to the previouse node
static inline void *head_to_prev(void *ptr) {
    return head_to_payload(ptr);
}

// Transform from head to the pointer pointing to the next node
static inline void *head_to_next(void *ptr) {
    return (char *)head_to_payload(ptr) + PTRSIZE;
}

static inline void *payload_to_foot(void *ptr) {
    return (char *)ptr + read_size(payload_to_head(ptr));
}

// Transform to the next block in the heap
static inline void *next_block(void *ptr) {
    return (char *)payload_to_foot(ptr) + 2*HFSIZE;
}

// Transform to the previouse block in the heap 
static inline void *prev_block(void *ptr) {
    return (char *)ptr - 2*HFSIZE - read_size((char *)ptr - 2*HFSIZE);
}

// Insert a new node in the head of the free block doubled linked list
void insert_node(void *ptr) {
    void *prev = head_to_prev(ptr);
    void *next = head_to_next(ptr);
    *(void **)prev = NULL;
    *(void **)next = free_list_head;
    if(free_list_head != NULL) {
        *(void **)head_to_prev(free_list_head) = ptr;
    }
    free_list_head = ptr;
}

// Delete a node in the head of the free block doubled linked list
void delete_node(void *ptr) {
    void *prev = head_to_prev(ptr);
    void *next = head_to_next(ptr);
    if(*(void **)prev == NULL && *(void **)next == NULL) {
        free_list_head = NULL;
        return;
    }
    if(*(void **)prev == NULL) {
        *(void **)head_to_prev(*(void **)next) = NULL;
        free_list_head = *(void **)next;
        return;
    }
    if(*(void **)next == NULL) {
        *(void **)head_to_next(*(void **)prev) = NULL;
        return;
    }
    *(void **)head_to_next(*(void **)prev) = *(void **)next;
    *(void **)head_to_prev(*(void **)next) = *(void **)prev;
}

// Merge the adjacent free blocks into one free block 
void merge_free_block(void *ptr) {
    size_t size = read_size(payload_to_head(ptr));
    size_t prev_alloc = 1;
    size_t next_alloc = 1;
    // The case that this block is the bottom block in the heap
    if(payload_to_head(ptr) != base) {
        prev_alloc = read_alloc(payload_to_head(prev_block(ptr)));
    }
    // The case that this block is the top block in the heap
    if(payload_to_foot(ptr) != end) {
        next_alloc = read_alloc(payload_to_head(next_block(ptr)));
    }
    // The case that the previouse block is allocated and the next one is free
    if(prev_alloc && !next_alloc) {
        void *next_ptr = next_block(ptr);
        size += read_size(payload_to_head(next_ptr)) + 2*HFSIZE;
        write_size(payload_to_head(ptr), size, 0);
        write_size(payload_to_foot(ptr), size, 0);
        delete_node(payload_to_head(next_ptr));
        insert_node(payload_to_head(ptr));
        return;
    }
    // The case that the previouse block is free and the next one is allocated
    if(!prev_alloc && next_alloc) {
        void *prev_ptr = prev_block(ptr);
        size += read_size(payload_to_head(prev_ptr)) + 2*HFSIZE;
        write_size(payload_to_foot(ptr), size, 0);
        write_size(payload_to_head(prev_ptr), size, 0);
        delete_node(payload_to_head(prev_ptr));
        insert_node(payload_to_head(prev_ptr));
        return;
    }
    // The case that both previouse and the next blocks are free
    if(!prev_alloc && !next_alloc) {
        void *prev_ptr = prev_block(ptr);
        void *next_ptr = next_block(ptr);
        size += read_size(payload_to_head(prev_ptr)) + read_size(payload_to_head(next_ptr)) + 4*HFSIZE;
        write_size(payload_to_head(prev_ptr), size, 0);
        write_size(payload_to_foot(next_ptr), size, 0);
        delete_node(payload_to_head(next_ptr));
        delete_node(payload_to_head(prev_ptr));
        insert_node(payload_to_head(prev_ptr));
        return;
    }
    // The case that both previouse and the next blocks are allocated
    insert_node(payload_to_head(ptr));
}

/*
 * Find the first block in the free list whose size is larger than the given size and return its position
 * If this block doesn't exist, return NULL
 */
void *find_free_block(size_t size) {
    void *cur_ptr = free_list_head;
    while(cur_ptr != NULL) {
        size_t nodesz = read_size(cur_ptr);
        if(nodesz >= size) {
            return head_to_payload(cur_ptr);
        }
        cur_ptr = *(void **)head_to_next(cur_ptr);
    }
    return NULL;
}

/*
 * Allocate a free block with size.
 * If the actually free size of this block is sufficiently larger than the size
 * I will split this block into one allocated block and the other free block
 */
void allocate(void *ptr, size_t size) {
    size_t freesz = read_size(payload_to_head(ptr));
    // The case that I split this block into one allocated block and the other free block
    if(freesz - size >= 2*HFSIZE + 2*PTRSIZE) {
        write_size(payload_to_head(ptr), size, 1);
        write_size(payload_to_foot(ptr), size, 1);
        void *free_ptr = next_block(ptr);
        write_size(payload_to_head(free_ptr), freesz - size - 2*HFSIZE, 0);
        write_size(payload_to_foot(free_ptr), freesz - size - 2*HFSIZE, 0);
        delete_node(payload_to_head(ptr));
        insert_node(payload_to_head(free_ptr));
    } else {
        write_size(payload_to_head(ptr), freesz, 1);
        write_size(payload_to_foot(ptr), freesz, 1);
        delete_node(payload_to_head(ptr));
    }
}

/*
 * Initialize the heap with one heap segment
 * Initialize the base and end pointer, the original free block and the head of the free linked list
 */
bool myinit() {
    base = init_heap_segment(1);
    if(base == NULL) {
        return false;
    }
    end = (char *)base + PAGE_SIZE - HFSIZE;
    write_size(base, PAGE_SIZE - 2*HFSIZE, 0);
    write_size(end, PAGE_SIZE - 2*HFSIZE, 0);
    free_list_head = base;
    void *prev = head_to_prev(base);
    void *next = head_to_next(base);
    *(void **)prev = NULL;
    *(void **)next = NULL;
    return true;
}

/*
 * Allocate the block
 */
void *mymalloc(size_t requestedsz) {
    if(requestedsz == 0 || requestedsz > INT_MAX) {
        return NULL;
    }
    //size must be adjusted to be at least 2*PTRSIZE so that they can hold two pointers and must be a multipy of ALIGNMENT
    size_t size = (requestedsz <= 2*PTRSIZE)? 2*PTRSIZE:roundup(requestedsz, ALIGNMENT);
    void *ptr = find_free_block(size);
    if(ptr != NULL) {
        allocate(ptr, size);
        return ptr;
    }
    // The case that we must extend the heap segment
    size_t end_size = 0;
    ptr = (char *)end + 2*HFSIZE;
    if(read_alloc(end) == 0) {
        end_size = read_size(end);
        ptr = (char *)end - end_size;
    }
    // Calculate the number of the new pages that we require to hold the new allocated block
    size_t num_pages = roundup(size - end_size + 2*HFSIZE, PAGE_SIZE)/PAGE_SIZE;
    void *newbase = extend_heap_segment(num_pages);
    if(newbase == NULL) {
        return NULL;
    }
    // Adjust the end pointer and initialize and merge the new free block
    end = (char *)newbase + PAGE_SIZE*num_pages - HFSIZE;
    write_size(newbase, PAGE_SIZE*num_pages - 2*HFSIZE, 0);
    write_size(end, PAGE_SIZE*num_pages - 2*HFSIZE, 0);
    merge_free_block(head_to_payload(newbase));
    allocate(ptr, size);
    return ptr;
}

/*
 * Free the block
 */ 
void myfree(void *ptr) {
    if(ptr == NULL) {
        return;
    }
    size_t size = read_size(payload_to_head(ptr));
    write_size(payload_to_head(ptr), size, 0);
    write_size(payload_to_foot(ptr), size, 0);
    // Merge the free blocks with potential previouse one or next one
    merge_free_block(ptr);
}

/*
 * Reallocate the block
 */
void *myrealloc(void *old_ptr, size_t newsz) {
    if(old_ptr == NULL) {
        return mymalloc(newsz);
    }
    if(newsz == 0) {
        myfree(old_ptr);
        return NULL;
    }
    size_t oldsz = read_size(payload_to_head(old_ptr));
    // New size must be adjusted to be at least 2*PTRSIZE so that they can hold two pointers and must be a multipy of ALIGNMENT
    newsz = (newsz <= 2*PTRSIZE)? 2*PTRSIZE:roundup(newsz, ALIGNMENT);
    if(newsz <= oldsz) {
        /* 
         * The case that new size is sufficiently larger than the old size so that
         * we can split the original block into one allocated block and the other free block
        */
        if(oldsz - newsz >= 2*HFSIZE + 2*PTRSIZE) {
            write_size(payload_to_head(old_ptr), newsz, 1);
            write_size(payload_to_foot(old_ptr), newsz, 1);
            void *free_ptr = next_block(old_ptr);
            write_size(payload_to_head(free_ptr), oldsz - newsz - 2*HFSIZE, 0);
            write_size(payload_to_foot(free_ptr), oldsz - newsz - 2*HFSIZE, 0);
            merge_free_block(free_ptr);
        }
        return old_ptr;
    }
    // The case that we must free the orignal one, allocate a new one and copy the original content
    char temp[oldsz];
    memcpy(temp, old_ptr, oldsz);
    myfree(old_ptr);
    void *new_ptr = mymalloc(newsz);
    memcpy(new_ptr, temp, oldsz);
    return new_ptr;
}

/*
 * Validate the correctness of the heap
 */
bool validate_heap() {
    // Check if the base is right
    if(base != heap_segment_start()) {
        printf("The base is ont right\n");
        return false;
    }
    // Check if the end is right
    if(end != (char *)base + heap_segment_size() - HFSIZE) {
        printf("The end is not right\n");
        return false;
    }
    void *cur = free_list_head;
    if(cur != NULL && *(void **)head_to_prev(cur) != NULL) {
        printf("The head of the free list is not right\n");
        return false;
    }
    // Check if the size and alloc information in the head and foot of the block in the free linked list is right
    while(cur != NULL) {
        size_t size_1 = read_size(cur);
        void *foot = payload_to_foot(head_to_payload(cur));
        size_t size_2 = read_size(foot);
        if(size_1 != size_2) {
            printf("The size is not matched correctly in the free linked list\n");
            return false;
        }
        size_t alloc_1 = read_alloc(cur);
        size_t alloc_2 = read_alloc(foot);
        if(alloc_1 != 0 || alloc_2 != 0) {
            printf("The alloc is not set to free in the free linked list\n");
            return false;
        }
        cur = *(void **)head_to_next(cur);
    }
    return true;
}
