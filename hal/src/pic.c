/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: pic.c
About: 8259 PIC remap (moving IRQ0-15 off the CPU exception vector
       range they collide with at power-on) and end-of-interrupt
       acknowledgement.
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT)
------------------------------------------------------------
*/

#include "pic.h"
#include "io_ports.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_ICW4 0x01
#define ICW1_INIT 0x10
#define ICW4_8086 0x01

static void io_wait(void)
{
    /* Port 0x80 is used by POST diagnostics on real hardware and nothing
       reads it back here; writing to it just burns a bus cycle so the two
       outb()s below don't outrun the (very old, slow) PIC hardware. */
    outb(0x80, 0);
}

void pic_remap(int offset1, int offset2)
{
    unsigned char mask1 = inb(PIC1_DATA);
    unsigned char mask2 = inb(PIC2_DATA);

    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4); io_wait();

    outb(PIC1_DATA, (unsigned char)offset1); io_wait();
    outb(PIC2_DATA, (unsigned char)offset2); io_wait();

    outb(PIC1_DATA, 4); io_wait(); /* tell master a slave sits on IRQ2 */
    outb(PIC2_DATA, 2); io_wait(); /* tell slave its cascade identity */

    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* restore whatever was masked before (keeps this idempotent / order-
       independent relative to individual IRQ enabling elsewhere) */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(int irq)
{
    if (irq >= 8)
        outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}
