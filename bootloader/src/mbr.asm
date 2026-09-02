; ------------------------------------------------------------
; Author: Subhajit Halder
; Date Created: 2026-08-31
; Date Last Modified: 2026-08-31
; Module: Bootloader
; File: mbr.asm
; About: Real BIOS/MBR boot sector -- replaces GRUB for a disk that has
;        actually been installed (Stage 5). BIOS loads this at 0000:7C00
;        in 16-bit real mode and jumps to it with DL = boot drive number.
;        Job: load Stage 2 (bootloader/src/stage2.asm) from a fixed LBA
;        via INT13h extended (LBA) reads, then jump to it. Nothing here
;        understands filesystems or partitions -- that's Stage 2/the
;        kernel's job. Bytes 446-509 (the classic MBR partition table)
;        are intentionally left zero here; kernel/src/mbr.c's mbr_write()
;        fills that region in separately without touching this bootstrap
;        code, so re-partitioning never requires re-installing the loader.
; Revisions:
; - 2026-08-31  Initial creation (Stage 4 of the FAT32/MBR/bootloader/
;               installer roadmap)
; ------------------------------------------------------------

BITS 16
ORG 0x7C00

%ifndef STAGE2_LBA
%define STAGE2_LBA 1
%endif
%ifndef STAGE2_SECTORS
%define STAGE2_SECTORS 16
%endif

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov byte  [dap.size], 0x10
    mov byte  [dap.reserved], 0
    mov word  [dap.count], STAGE2_SECTORS
    mov word  [dap.offset], 0x7E00
    mov word  [dap.segment], 0x0000
    mov dword [dap.lba_lo], STAGE2_LBA
    mov dword [dap.lba_hi], 0

    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error

    mov dl, [boot_drive]        ; stage2 expects the boot drive in DL too
    jmp 0x0000:0x7E00

disk_error:
    mov si, err_msg
.print:
    lodsb
    or al, al
    jz .hang
    mov ah, 0x0E
    int 0x10
    jmp .print
.hang:
    cli
    hlt
    jmp .hang

boot_drive: db 0
err_msg:    db "MBR: stage2 disk read failed", 0

align 4
dap:
    .size:     db 0
    .reserved: db 0
    .count:    dw 0
    .offset:   dw 0
    .segment:  dw 0
    .lba_lo:   dd 0
    .lba_hi:   dd 0

times 446 - ($ - $$) db 0      ; pad up to the partition-table boundary
                                ; (assembly fails loudly here if the code
                                ; above ever grows past the 446-byte budget)
times 510 - ($ - $$) db 0      ; partition table region, left zeroed --
                                ; mbr_write() fills this in later
dw 0xAA55
