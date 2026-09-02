/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: pit.h
About: Programs the 8253/8254 Programmable Interval Timer (channel 0,
       square-wave mode) at a fixed frequency and counts ticks via its
       IRQ0 handler -- this project's only clock source (pal_sleep(),
       hal_timer_get_ticks(), ATA/keyboard timeouts all derive from it).
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT)
------------------------------------------------------------
*/

#ifndef KERNEL_PIT_H
#define KERNEL_PIT_H

/* Programs PIT channel 0 for square-wave mode at `frequency` Hz and
   registers its IRQ0 tick handler. Call after irq_install()/pic_remap(),
   before enabling interrupts (sti). */
void pit_initialize(unsigned int frequency);

unsigned long pit_get_ticks(void);

#endif
