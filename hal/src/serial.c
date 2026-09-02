/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: serial.c
About: COM1 UART driver, polled, no IRQ -- 38400 baud, 8N1. The one
       output channel that existed before there was a working VGA/GUI,
       and still what hal_debug_print()/boot diagnostics use, since it's
       visible headlessly under QEMU (`-serial stdio`) without a display.
Revisions:
- 2026-08-30  Initial creation (Phase 1: boot + VGA/serial drivers)
------------------------------------------------------------
*/

#include "serial.h"
#include "io_ports.h"

#define COM1 0x3F8

void serial_initialize(void)
{
    outb(COM1 + 1, 0x00); /* disable UART interrupts */
    outb(COM1 + 3, 0x80); /* enable DLAB to set baud divisor */
    outb(COM1 + 0, 0x03); /* divisor low byte  -> 38400 baud */
    outb(COM1 + 1, 0x00); /* divisor high byte */
    outb(COM1 + 3, 0x03); /* 8 bits, no parity, one stop bit; DLAB off */
    outb(COM1 + 2, 0xC7); /* enable FIFO, clear, 14-byte threshold */
    outb(COM1 + 4, 0x0B); /* IRQs enabled, RTS/DSR set */
}

static int serial_transmit_empty(void)
{
    return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c)
{
    while (!serial_transmit_empty())
        ;
    outb(COM1, (unsigned char)c);
}

void serial_print(const char* str)
{
    while (*str)
        serial_putchar(*str++);
}
