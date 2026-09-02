/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-09-01
Module: Kernel
File: paging.c
About: Builds an identity-mapped (virtual == physical) page directory
       covering detected RAM up to a static 256MB cap, loads it into
       CR3, and flips CR0.PG. RAM beyond the cap is left unmapped --
       touching it page-faults into isr.c's handler, same as any other
       bad access, since there's no demand-paging to grow the map yet.
Revisions:
- 2026-08-30  Initial creation (Phase 3: PMM + paging)
- 2026-09-01  Added paging_get_identity_limit() (tracks the real mapped
              extent, not just the static cap) so hal/src/acpi.c can
              bounds-check firmware physical pointers before dereferencing
              them -- fixes a page fault found on a >256MB-RAM VM whose
              ACPI tables sat near the top of RAM, outside the old
              blindly-trusted map.
------------------------------------------------------------
*/

#include "paging.h"
#include "pmm.h"

#define PAGE_PRESENT  0x1
#define PAGE_WRITABLE 0x2

/*
   Static cap on how much RAM gets identity-mapped at boot: 256MB (64 page
   tables x 1024 entries x 4KB) comfortably covers QEMU's default and any
   modest real box this targets. RAM beyond the cap is left unmapped --
   touching it would page-fault into isr.c's unrecoverable handler, same as
   any other bad access, since there's no demand-paging to grow the map yet.
*/
#define IDENTITY_MAP_MAX_MB 256
#define BYTES_PER_TABLE (1024u * 4096u) /* one page table maps 4MB */
#define IDENTITY_MAP_TABLES (IDENTITY_MAP_MAX_MB / 4)

static unsigned int page_directory[1024] __attribute__((aligned(4096)));
static unsigned int page_tables[IDENTITY_MAP_TABLES][1024] __attribute__((aligned(4096)));
static unsigned int g_tables_mapped = 0;

unsigned long long paging_get_identity_limit(void)
{
    return (unsigned long long)g_tables_mapped * BYTES_PER_TABLE;
}

void paging_init(void)
{
    unsigned long long highest = pmm_get_highest_address();
    unsigned int tables_needed;
    unsigned int table, entry;

    /* highest / BYTES_PER_TABLE, rounded up -- kept as a shift+compare loop
       instead of real division so this doesn't need a libgcc 64-bit divide
       helper (see pmm.c's comment; -nostdlib means there's none linked). */
    tables_needed = 0;
    while ((unsigned long long)tables_needed * BYTES_PER_TABLE < highest
           && tables_needed < IDENTITY_MAP_TABLES)
        tables_needed++;

    if (tables_needed == 0)
        tables_needed = 1; /* always map at least the first 4MB (kernel + low memory) */

    g_tables_mapped = tables_needed;

    for (table = 0; table < tables_needed; table++)
    {
        for (entry = 0; entry < 1024; entry++)
        {
            unsigned int phys = (table * 1024u + entry) * 4096u;
            page_tables[table][entry] = phys | PAGE_PRESENT | PAGE_WRITABLE;
        }
        page_directory[table] = ((unsigned int)&page_tables[table]) | PAGE_PRESENT | PAGE_WRITABLE;
    }
    for (; table < 1024; table++)
        page_directory[table] = 0; /* not present -- any access here is a bug, let it page-fault */

    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        : "r"(page_directory)
        : "eax", "memory"
    );
}
