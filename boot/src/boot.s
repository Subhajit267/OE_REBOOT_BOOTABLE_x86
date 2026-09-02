; ------------------------------------------------------------
; Author: Subhajit Halder
; Date Created: 2026-08-30
; Date Last Modified: 2026-08-30
; Module: Boot
; File: boot.s
; About: Multiboot1-compliant entry stub. GRUB loads this at 1MB,
;        already in 32-bit protected mode with paging disabled —
;        we just need a stack and to hand off to C.
; Revisions:
; - 2026-08-30  Initial creation (Phase 1: "Hello Kernel" boot)
; ------------------------------------------------------------

MBALIGN  equ  1<<0             ; align loaded modules on page boundaries
MEMINFO  equ  1<<1             ; ask GRUB for a memory map
FLAGS    equ  MBALIGN | MEMINFO
MAGIC    equ  0x1BADB002       ; multiboot1 magic GRUB checks for
CHECKSUM equ -(MAGIC + FLAGS)  ; magic + flags + checksum must sum to 0

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384                 ; 16 KiB kernel stack
stack_top:

section .text
global _start
extern kernel_main
_start:
    mov esp, stack_top         ; no valid stack until this point

    push ebx                   ; multiboot info struct pointer
    push eax                   ; multiboot magic (0x2BADB002 if valid)
    call kernel_main

    cli
.hang:
    hlt
    jmp .hang
