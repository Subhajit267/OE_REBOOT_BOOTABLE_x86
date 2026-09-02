/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: vga.h
About: VGA text-mode console driver with a built-in ANSI/CSI escape
       interpreter (see vga.c) -- the bare-metal backend for hal.h's
       hal_console_* calls, which is what makes the existing hosted
       pal.h-color-macro UI render correctly with no terminal underneath.
Revisions:
- 2026-08-30  Initial creation (Phase 1: boot + VGA/serial drivers)
------------------------------------------------------------
*/

#ifndef HAL_VGA_H
#define HAL_VGA_H

void vga_initialize(void);
void vga_putchar(char c);
void vga_print(const char* str);
void vga_get_size(int* rows, int* cols);

#endif
