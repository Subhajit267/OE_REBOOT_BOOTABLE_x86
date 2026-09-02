/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: irq.h
About: Installs the 16 IRQ gates (vectors 32-47, after pic_remap() moves
       them off the CPU exception range) into the IDT, and provides
       irq_install_handler() so drivers (pit.c, keyboard.c) can register
       their own IRQ callback without this file knowing about them.
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT)
------------------------------------------------------------
*/

#ifndef KERNEL_IRQ_H
#define KERNEL_IRQ_H

#define IRQ_BASE 32 /* IDT vector the PIC is remapped to for IRQ0 (pic_remap) */

typedef void (*irq_handler_t)(void);

/* Installs the 16 IRQ gates (vectors 32-47) into the IDT. Call after
   idt_initialize() and pic_remap(). */
void irq_install(void);

void irq_install_handler(int irq, irq_handler_t handler);

#endif
