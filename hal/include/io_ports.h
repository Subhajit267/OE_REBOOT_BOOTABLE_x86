/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: io_ports.h
About: Shared x86 port I/O primitives (inb/outb/inw/outw) -- every
       hal/src driver goes through these rather than inlining its own.
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT),
              inb/outb only.
- 2026-08-30  Added inw/outw (Phase 5: ATA disk driver, 16-bit PIO).
------------------------------------------------------------
*/

#ifndef HAL_IO_PORTS_H
#define HAL_IO_PORTS_H

/* Shared x86 port I/O primitives -- every hal/src driver goes through
   these rather than inlining its own inb/outb. */

static inline void outb(unsigned short port, unsigned char val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port)
{
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(unsigned short port, unsigned short val)
{
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned short inw(unsigned short port)
{
    unsigned short ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#endif
