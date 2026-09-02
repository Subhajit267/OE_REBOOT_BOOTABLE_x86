/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: idt.h
About: 256-entry Interrupt Descriptor Table. idt_initialize() starts
       every vector absent; isr.c/irq.c fill in the ones actually
       handled (0-31 CPU exceptions, 32-47 IRQs) via the shared
       idt_set_gate().
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT)
------------------------------------------------------------
*/

#ifndef KERNEL_IDT_H
#define KERNEL_IDT_H

#define IDT_KERNEL_CS    0x08 /* matches gdt.c's flat ring0 code descriptor */
#define IDT_GATE_INT32   0x8E /* present, ring0, 32-bit interrupt gate */

void idt_initialize(void);
void idt_set_gate(int num, unsigned int base, unsigned short sel, unsigned char flags);

#endif
