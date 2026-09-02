/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: Kernel
File: multiboot.h
About: Subset of the multiboot1 info struct this kernel actually reads
       (memory map + basic upper/lower memory), used by pmm_init() to
       size the physical frame allocator. Populated either by GRUB
       directly, or synthesized from a real E820 map by the Stage-4
       bootloader (bootloader/src/stage2.asm) so both boot paths hand
       the kernel the exact same contract.
Revisions:
- 2026-08-30  Initial creation (Phase 3: PMM + paging)
------------------------------------------------------------
*/

#ifndef KERNEL_MULTIBOOT_H
#define KERNEL_MULTIBOOT_H

/* Subset of the multiboot1 info struct this kernel actually reads (memory
   map + basic upper/lower memory). Full spec has cmdline, modules, ELF/
   a.out symbol tables, VBE info, etc. -- add fields here if/when something
   needs them. */

#define MULTIBOOT_INFO_MEMORY 0x00000001
#define MULTIBOOT_INFO_MMAP   0x00000040

#define MULTIBOOT_MEMORY_AVAILABLE 1

struct multiboot_info
{
    unsigned int flags;

    unsigned int mem_lower;
    unsigned int mem_upper;

    unsigned int boot_device;
    unsigned int cmdline;

    unsigned int mods_count;
    unsigned int mods_addr;

    unsigned int syms[4];

    unsigned int mmap_length;
    unsigned int mmap_addr;

    /* remaining multiboot1 fields (drives, config table, boot loader name,
       APM, VBE) intentionally omitted -- not read anywhere yet */
} __attribute__((packed));

/* One variable-length entry in the mmap; `size` covers only the bytes AFTER
   itself, so advancing the list is `addr += entry->size + sizeof(entry->size)`,
   not sizeof(struct multiboot_mmap_entry). */
struct multiboot_mmap_entry
{
    unsigned int size;
    unsigned long long addr;
    unsigned long long len;
    unsigned int type;
} __attribute__((packed));

#endif
