/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: idt.c
About: 256-entry Interrupt Descriptor Table. idt_set_gate() is the
       shared primitive isr.c/irq.c both call to install their handlers;
       idt_initialize() starts every vector absent so an unhandled one
       correctly triple-faults instead of silently misbehaving.
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT)
------------------------------------------------------------
*/

#include "idt.h"

struct idt_entry
{
    unsigned short base_low;
    unsigned short sel;
    unsigned char  always0;
    unsigned char  flags;
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr
{
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed));

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

void idt_set_gate(int num, unsigned int base, unsigned short sel, unsigned char flags)
{
    idt[num].base_low  = (unsigned short)(base & 0xFFFF);
    idt[num].base_high = (unsigned short)((base >> 16) & 0xFFFF);
    idt[num].sel       = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

void idt_initialize(void)
{
    int i;

    idtp.limit = (unsigned short)(sizeof(idt) - 1);
    idtp.base  = (unsigned int)&idt;

    /* Every vector starts absent; isr_install()/irq_install() fill in the
       ones the kernel actually handles (0-31 exceptions, 32-47 IRQs). An
       unhandled vector firing hits a null IDT entry -> triple fault, which
       is the correct failure mode until there's a reason to handle more. */
    for (i = 0; i < IDT_ENTRIES; i++)
        idt_set_gate(i, 0, 0, 0);

    __asm__ volatile ("lidt (%0)" : : "r"(&idtp));
}
