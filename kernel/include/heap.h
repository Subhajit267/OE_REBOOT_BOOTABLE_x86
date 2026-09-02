/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: Kernel
File: heap.h
About: kmalloc/krealloc/kfree over a fixed 1MB static arena (first-fit
       free list) -- backs every pal_alloc() call in pal/src/pal_kernel.c.
       Not PMM-backed/growable yet; that's future work if something ever
       needs more than 1MB of kernel heap at once.
Revisions:
- 2026-08-30  Initial creation (Phase 6: pal_kernel.c bring-up)
------------------------------------------------------------
*/

#ifndef KERNEL_HEAP_H
#define KERNEL_HEAP_H

/* First call auto-initializes on first kmalloc(), so there's no required
   boot-sequence ordering -- but callers that care about a clean starting
   arena (tests, etc.) can still call it explicitly. */
void heap_init(void);

void* kmalloc(unsigned int size);
void* krealloc(void* ptr, unsigned int size);
void  kfree(void* ptr);

#endif
