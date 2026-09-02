/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: Kernel
File: pmm.h
About: Physical memory manager -- a bitmap frame allocator (1 bit per
       4KB frame) sized for the full 4GB 32-bit address space. Backs
       kernel/src/paging.c's identity map sizing and pal_get_oe_info()'s
       RAM reporting; does NOT back heap.c's kmalloc (a separate fixed
       static arena, unrelated to this).
Revisions:
- 2026-08-30  Initial creation (Phase 3: PMM + paging)
------------------------------------------------------------
*/

#ifndef KERNEL_PMM_H
#define KERNEL_PMM_H

#define PMM_PAGE_SIZE 4096

/* Parses the multiboot memory map at mb_info_addr and builds the frame
   bitmap: every frame starts reserved, available (type 1) mmap regions get
   freed, then the kernel's own image, the mmap/info structures themselves,
   and the low-1MB BIOS/real-mode area get re-reserved regardless of what
   the map claims about them. Call once, before paging_init(). */
void pmm_init(unsigned long mb_info_addr);

/* Returns a physical, page-aligned frame, or 0 if none remain. */
void* pmm_alloc_frame(void);

void pmm_free_frame(void* addr);

unsigned int pmm_get_free_frame_count(void);

/* One past the highest byte address any "available" mmap entry claims --
   paging_init() uses this to size the identity map. */
unsigned long long pmm_get_highest_address(void);

#endif
