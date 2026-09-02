/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: isr.h
About: Installs the 32 CPU exception gates (vectors 0-31) into the IDT,
       via GCC's __attribute__((interrupt)) handlers rather than
       hand-rolled assembly stubs.
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT)
------------------------------------------------------------
*/

#ifndef KERNEL_ISR_H
#define KERNEL_ISR_H

/* Installs the 32 CPU exception gates (vectors 0-31) into the IDT. Call
   after idt_initialize(). */
void isr_install(void);

#endif
