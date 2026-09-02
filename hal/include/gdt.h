/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: gdt.h
About: Installs this kernel's own flat 4GB null/ring0-code/ring0-data
       GDT (GRUB leaves some GDT loaded, but its layout isn't part of
       the multiboot contract) so 0x08/0x10 are fixed, known selector
       constants for idt.c's interrupt gates and segment reloads.
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT)
------------------------------------------------------------
*/

#ifndef KERNEL_GDT_H
#define KERNEL_GDT_H

void gdt_initialize(void);

#endif
