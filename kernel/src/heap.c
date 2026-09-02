/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: Kernel
File: heap.c
About: First-fit free-list allocator over a fixed static 1MB arena.
       kmalloc() auto-initializes the arena on first call, so there's no
       required boot-sequence ordering. kfree() coalesces adjacent free
       blocks; krealloc() grows in place when there's room, otherwise
       allocates fresh and copies (no shrink-split, for simplicity).
Revisions:
- 2026-08-30  Initial creation (Phase 6: pal_kernel.c bring-up)
------------------------------------------------------------
*/

#include "heap.h"

/*
   Fixed static arena, first-fit free-list allocator -- plenty for what
   pal_kernel.c backs today (string dup/alloc calls, one file's contents
   at a time). Growing this onto PMM-allocated pages on demand is future
   work if something ever needs more than 1MB of kernel heap at once.
*/
#define HEAP_SIZE (1024u * 1024u)

static unsigned char heap_arena[HEAP_SIZE] __attribute__((aligned(16)));

struct block_header
{
    unsigned int size; /* payload size, not including this header */
    int free;
    struct block_header* next;
};

#define HEADER_SIZE (sizeof(struct block_header))

static struct block_header* heap_head;
static int heap_initialized;

void heap_init(void)
{
    heap_head = (struct block_header*)heap_arena;
    heap_head->size = HEAP_SIZE - (unsigned int)HEADER_SIZE;
    heap_head->free = 1;
    heap_head->next = 0;
    heap_initialized = 1;
}

static void split_block(struct block_header* block, unsigned int size)
{
    struct block_header* remainder;

    if (block->size < size + (unsigned int)HEADER_SIZE + 16u)
        return; /* remainder too small to be worth splitting off */

    remainder = (struct block_header*)((unsigned char*)block + HEADER_SIZE + size);
    remainder->size = block->size - size - (unsigned int)HEADER_SIZE;
    remainder->free = 1;
    remainder->next = block->next;

    block->size = size;
    block->next = remainder;
}

void* kmalloc(unsigned int size)
{
    struct block_header* b;

    if (!heap_initialized)
        heap_init();

    size = (size + 15u) & ~15u; /* 16-byte-align payloads */

    for (b = heap_head; b; b = b->next)
    {
        if (b->free && b->size >= size)
        {
            split_block(b, size);
            b->free = 0;
            return (unsigned char*)b + HEADER_SIZE;
        }
    }
    return 0; /* heap exhausted */
}

static void coalesce(void)
{
    struct block_header* b = heap_head;

    while (b && b->next)
    {
        if (b->free && b->next->free)
        {
            b->size += (unsigned int)HEADER_SIZE + b->next->size;
            b->next = b->next->next;
        }
        else
        {
            b = b->next;
        }
    }
}

void kfree(void* ptr)
{
    struct block_header* b;

    if (!ptr)
        return;

    b = (struct block_header*)((unsigned char*)ptr - HEADER_SIZE);
    b->free = 1;
    coalesce();
}

void* krealloc(void* ptr, unsigned int size)
{
    struct block_header* b;
    void* new_ptr;
    unsigned char* d;
    unsigned char* s;
    unsigned int i;

    if (!ptr)
        return kmalloc(size);
    if (size == 0)
    {
        kfree(ptr);
        return 0;
    }

    b = (struct block_header*)((unsigned char*)ptr - HEADER_SIZE);
    size = (size + 15u) & ~15u;
    if (b->size >= size)
        return ptr; /* already big enough; no shrink-split for simplicity */

    new_ptr = kmalloc(size);
    if (!new_ptr)
        return 0;

    d = (unsigned char*)new_ptr;
    s = (unsigned char*)ptr;
    for (i = 0; i < b->size; i++)
        d[i] = s[i];

    kfree(ptr);
    return new_ptr;
}
