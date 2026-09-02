/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: pic.h
About: Remaps the two 8259 PICs so IRQ0-15 land on IDT vectors that
       don't collide with the CPU exception vectors (their power-on
       default of 0x08-0x0F/0x70-0x77 does), and end-of-interrupt
       acknowledgement.
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT)
------------------------------------------------------------
*/

#ifndef KERNEL_PIC_H
#define KERNEL_PIC_H

/* Remaps the two 8259 PICs so IRQ0-15 land on IDT vectors offset1..offset1+7
   and offset2..offset2+7 instead of their power-on default of 0x08-0x0F /
   0x70-0x77, which collide head-on with the CPU exception vectors. */
void pic_remap(int offset1, int offset2);

void pic_send_eoi(int irq);

#endif
