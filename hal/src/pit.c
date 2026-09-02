/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: pit.c
About: Programs PIT channel 0 for square-wave mode at HAL_TIMER_HZ and
       counts ticks via its IRQ0 handler -- this project's only clock
       source (pal_sleep(), hal_timer_get_ticks(), ATA/keyboard timeouts).
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT)
------------------------------------------------------------
*/

#include "pit.h"
#include "io_ports.h"
#include "irq.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_FREQ 1193182u /* fixed input clock of the 8253/8254 PIT */

static volatile unsigned long pit_ticks;

static void pit_handler(void)
{
    pit_ticks++;
}

void pit_initialize(unsigned int frequency)
{
    unsigned int divisor = PIT_BASE_FREQ / frequency;

    outb(PIT_COMMAND, 0x36); /* channel 0, lobyte/hibyte access, mode 3 */
    outb(PIT_CHANNEL0, (unsigned char)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (unsigned char)((divisor >> 8) & 0xFF));

    irq_install_handler(0, pit_handler);
}

unsigned long pit_get_ticks(void)
{
    return pit_ticks;
}
