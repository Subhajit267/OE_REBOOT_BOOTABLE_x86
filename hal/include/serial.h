/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: serial.h
About: COM1 driver, polled/no IRQ. Kept alongside the VGA driver from
       day one -- a bare-metal kernel with no GUI and no debugger
       attached is otherwise a black box; this is what let the boot get
       verified headlessly under QEMU, and still backs hal_debug_print().
Revisions:
- 2026-08-30  Initial creation (Phase 1: boot + VGA/serial drivers)
------------------------------------------------------------
*/

#ifndef KERNEL_SERIAL_H
#define KERNEL_SERIAL_H

/*
   COM1 driver. Kept alongside the VGA driver from day one because a bare-
   metal kernel with no GUI and no debugger attached is otherwise a black
   box — this is what let the boot get verified headlessly under QEMU.
*/

void serial_initialize(void);
void serial_putchar(char c);
void serial_print(const char* str);

#endif
