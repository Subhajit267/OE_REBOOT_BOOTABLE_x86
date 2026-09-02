/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: gdt.c
About: Installs a flat 4GB null/ring0-code/ring0-data GDT and reloads
       every segment register through it via a far jump (CS can only be
       reloaded that way -- a plain mov can't).
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT)
------------------------------------------------------------
*/

#include "gdt.h"

/*
   GRUB leaves us in protected mode with *some* GDT already loaded, but its
   layout isn't part of the multiboot contract -- relying on it would make
   every later selector value (segment regs, IDT gate selectors) a guess.
   Installing our own flat 4GB null/code/data GDT here makes 0x08/0x10 fixed,
   known constants for the rest of the kernel (idt.c's gate selector, the
   segment reloads below).
*/

struct gdt_entry
{
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char  base_mid;
    unsigned char  access;
    unsigned char  granularity;
    unsigned char  base_high;
} __attribute__((packed));

struct gdt_ptr
{
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed));

#define GDT_ENTRIES 3

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdtp;

static void gdt_set_gate(int num, unsigned int base, unsigned int limit,
                          unsigned char access, unsigned char gran)
{
    gdt[num].base_low    = (unsigned short)(base & 0xFFFF);
    gdt[num].base_mid    = (unsigned char)((base >> 16) & 0xFF);
    gdt[num].base_high   = (unsigned char)((base >> 24) & 0xFF);

    gdt[num].limit_low   = (unsigned short)(limit & 0xFFFF);
    gdt[num].granularity = (unsigned char)(((limit >> 16) & 0x0F) | (gran & 0xF0));

    gdt[num].access      = access;
}

void gdt_initialize(void)
{
    gdtp.limit = (unsigned short)(sizeof(gdt) - 1);
    gdtp.base  = (unsigned int)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                    /* null descriptor, mandatory */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);      /* 0x08: ring0 code, flat 4GB */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);      /* 0x10: ring0 data, flat 4GB */

    /*
       lgdt loads the new table, but CS only takes effect on a far jump/call/
       ret/iret -- a plain mov can't reload it. The local label lets the jump
       target sit right after itself so execution just continues in-place,
       now under the new CS.
    */
    __asm__ volatile (
        "lgdt (%0)\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        :
        : "r"(&gdtp)
        : "eax", "memory"
    );
}
