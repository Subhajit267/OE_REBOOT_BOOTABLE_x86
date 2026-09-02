/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: Kernel
File: pmm.c
About: Bitmap physical frame allocator. pmm_init() parses the multiboot
       memory map: every frame starts reserved, available regions get
       freed, then the low-1MB BIOS/real-mode area, the kernel's own
       loaded image (kernel_start/kernel_end, linker.ld), and the
       mmap/info structures themselves get re-reserved regardless of
       what the map claims about them.
Revisions:
- 2026-08-30  Initial creation (Phase 3: PMM + paging)
------------------------------------------------------------
*/

#include "pmm.h"
#include "multiboot.h"

/*
   64-bit division/modulo by a non-constant pulls in __udivdi3 from libgcc,
   which isn't linked (-nostdlib, no libgcc.a on the link line). Every
   addr<->frame conversion below uses >>12 (PMM_PAGE_SIZE is 4096) instead
   of "/ PMM_PAGE_SIZE" so it stays plain shifts/compares the compiler can
   inline -- no division at all, 64-bit or otherwise.
*/

#define MAX_FRAMES (0x100000000ULL >> 12) /* 4GB address ceiling / 4KB pages */

static unsigned char frame_bitmap[MAX_FRAMES / 8];
static unsigned long long highest_address;

extern char kernel_start[];
extern char kernel_end[];

static void set_used(unsigned int frame)
{
    if (frame < MAX_FRAMES)
        frame_bitmap[frame / 8] |= (unsigned char)(1u << (frame % 8));
}

static void set_free(unsigned int frame)
{
    if (frame < MAX_FRAMES)
        frame_bitmap[frame / 8] &= (unsigned char)~(1u << (frame % 8));
}

static int is_used(unsigned int frame)
{
    return frame_bitmap[frame / 8] & (1u << (frame % 8));
}

static void mark_range(unsigned long long addr, unsigned long long len, int used)
{
    unsigned long long start = addr & ~(unsigned long long)0xFFF; /* page-align down */
    unsigned long long end   = addr + len;
    unsigned long long f;

    for (f = start; f < end; f += PMM_PAGE_SIZE)
    {
        unsigned int frame = (unsigned int)(f >> 12);
        if (used)
            set_used(frame);
        else
            set_free(frame);
    }
}

void pmm_init(unsigned long mb_info_addr)
{
    struct multiboot_info *info = (struct multiboot_info*)mb_info_addr;
    unsigned int i;

    for (i = 0; i < sizeof(frame_bitmap); i++)
        frame_bitmap[i] = 0xFF; /* pessimistic: nothing is free until the mmap says so */

    highest_address = 0;

    if (info->flags & MULTIBOOT_INFO_MMAP)
    {
        unsigned long addr = info->mmap_addr;
        unsigned long end  = info->mmap_addr + info->mmap_length;

        while (addr < end)
        {
            struct multiboot_mmap_entry *e = (struct multiboot_mmap_entry*)addr;

            if (e->type == MULTIBOOT_MEMORY_AVAILABLE)
            {
                mark_range(e->addr, e->len, 0);

                unsigned long long region_end = e->addr + e->len;
                if (region_end > highest_address)
                    highest_address = region_end;
            }

            addr += e->size + sizeof(e->size);
        }
    }

    /* Re-reserve what the mmap either doesn't cover or doesn't know is ours:
       the real-mode IVT/BDA/VGA/BIOS area below 1MB, our own loaded image,
       and the mmap/info structures GRUB left for us to read -- all regardless
       of what type the map lists them as, since handing any of these out
       would corrupt something still in use. */
    mark_range(0, 0x100000, 1);
    mark_range((unsigned long)kernel_start, (unsigned long)(kernel_end - kernel_start), 1);
    if (info->flags & MULTIBOOT_INFO_MMAP)
        mark_range(info->mmap_addr, info->mmap_length, 1);
}

void* pmm_alloc_frame(void)
{
    unsigned int frame;

    for (frame = 0; frame < MAX_FRAMES; frame++)
    {
        if (!is_used(frame))
        {
            set_used(frame);
            return (void*)(frame << 12);
        }
    }

    return 0; /* out of memory */
}

void pmm_free_frame(void* addr)
{
    set_free((unsigned int)((unsigned long)addr >> 12));
}

unsigned int pmm_get_free_frame_count(void)
{
    unsigned int frame;
    unsigned int free_count = 0;

    for (frame = 0; frame < MAX_FRAMES; frame++)
        if (!is_used(frame))
            free_count++;

    return free_count;
}

unsigned long long pmm_get_highest_address(void)
{
    return highest_address;
}
