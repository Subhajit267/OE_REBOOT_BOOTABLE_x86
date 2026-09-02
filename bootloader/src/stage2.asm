; ------------------------------------------------------------
; Author: Subhajit Halder
; Date Created: 2026-08-31
; Date Last Modified: 2026-08-31
; Module: Bootloader
; File: stage2.asm
; About: Loaded by mbr.asm at 0000:7E00, 16-bit real mode. Replicates
;        what GRUB does for the ISO path, without GRUB: gather a real
;        E820 memory map, load the kernel's flat image from a fixed LBA
;        range into low memory, switch to 32-bit protected mode, copy the
;        kernel up to its linked 1MB load address, build a synthetic
;        multiboot_info struct (kernel/include/multiboot.h) from the E820
;        data, then jump straight into the kernel's real entry point with
;        EAX/EBX set exactly as boot.s's _start expects them from GRUB.
;        kernel_main/pmm.c need zero changes -- this only has to honor
;        the same handoff contract GRUB already provides.
; Revisions:
; - 2026-08-31  Initial creation (Stage 4 of the FAT32/MBR/bootloader/
;               installer roadmap)
; ------------------------------------------------------------

BITS 16
ORG 0x7E00

%ifndef KERNEL_LBA
%define KERNEL_LBA 17
%endif
%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 512
%endif
%ifndef KERNEL_ENTRY
%define KERNEL_ENTRY 0x100010
%endif

KERNEL_STAGE_SEG  equ 0x1000      ; physical 0x10000 -- real-mode staging area for the raw kernel bytes
KERNEL_LOAD_ADDR  equ 0x100000    ; 1MB -- kernel/linker.ld's real link address
MMAP_BUFFER       equ 0xA000      ; low memory, past stage2's own footprint, ES=0-reachable (<0x10000)
MB_INFO_ADDR      equ 0xC000      ; multiboot_info struct, right after the mmap buffer

CODE_SEG equ 0x08
DATA_SEG equ 0x10

start2:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl

    call detect_memory_e820
    call load_kernel_image
    call enable_a20

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEG:protected_mode_entry

; ============ 16-bit real-mode helpers ============

; INT 15h / EAX=0xE820 memory map. Writes multiboot_mmap_entry-format
; records (dd size=20; dd addr_lo; dd addr_hi; dd len_lo; dd len_hi; dd type)
; back-to-back at MMAP_BUFFER; mmap_count holds how many. ES must be 0 for
; ES:DI to land on the (< 0x10000) MMAP_BUFFER address correctly.
detect_memory_e820:
    pusha
    mov edi, MMAP_BUFFER + 4       ; leave room for entry 0's leading `size` field
    xor ebx, ebx
    mov dword [mmap_count], 0
.loop:
    mov eax, 0xE820
    mov edx, 0x534D4150            ; 'SMAP'
    mov ecx, 20                    ; addr(8)+len(8)+type(4) -- matches multiboot_mmap_entry's payload exactly
    int 0x15
    jc .done                       ; CF set: end of list, or unsupported
    cmp eax, 0x534D4150
    jne .done
    cmp ecx, 20
    jb .done                       ; malformed/short entry -- stop rather than trust partial data

    mov dword [edi - 4], 20        ; this entry's multiboot_mmap_entry.size field
    inc dword [mmap_count]
    add edi, 24                    ; 4 (size) + 20 (payload) -- next entry's payload start

    test ebx, ebx
    jnz .loop
.done:
    popa
    ret

; Reads KERNEL_SECTORS sectors from LBA KERNEL_LBA into physical
; KERNEL_STAGE_SEG:0000 (0x10000), 64 sectors (32KB) at a time so no single
; INT13h extended read call gets close to any BIOS chunk-size limit.
load_kernel_image:
    pusha
    mov word  [kseg], KERNEL_STAGE_SEG
    mov dword [klba], KERNEL_LBA
    mov word  [kremain], KERNEL_SECTORS
.chunk:
    mov ax, [kremain]
    test ax, ax
    jz .done
    cmp ax, 64
    jbe .have_count
    mov ax, 64
.have_count:
    mov [dap2.count], ax

    mov byte  [dap2.size], 0x10
    mov byte  [dap2.reserved], 0
    mov word  [dap2.offset], 0x0000
    mov bx, [kseg]
    mov word  [dap2.segment], bx
    mov eax, [klba]
    mov dword [dap2.lba_lo], eax
    mov dword [dap2.lba_hi], 0

    mov si, dap2
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc kernel_disk_error

    movzx eax, word [dap2.count]
    add [klba], eax
    mov ax, [dap2.count]
    sub [kremain], ax
    shl ax, 5                       ; sectors * 512 / 16 = sectors * 32 paragraphs
    add [kseg], ax
    jmp .chunk
.done:
    popa
    ret

enable_a20:
    in al, 0x92
    or al, 2
    out 0x92, al
    ret

kernel_disk_error:
    mov si, kerr_msg
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

boot_drive:  db 0
kseg:        dw 0
klba:        dd 0
kremain:     dw 0
mmap_count:  dd 0
kerr_msg:    db "Stage2: kernel disk read failed", 0

align 4
dap2:
    .size:     db 0
    .reserved: db 0
    .count:    dw 0
    .offset:   dw 0
    .segment:  dw 0
    .lba_lo:   dd 0
    .lba_hi:   dd 0

align 8
gdt_start:
    dq 0x0000000000000000          ; null descriptor
    dq 0x00CF9A000000FFFF          ; code: base=0 limit=4G 32-bit, present/ring0/exec/readable
    dq 0x00CF92000000FFFF          ; data: base=0 limit=4G 32-bit, present/ring0/write
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ============ 32-bit protected mode ============

BITS 32
protected_mode_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000                ; well above everything else used so far, below the kernel's own stack setup in boot.s's _start

    ; Copy the staged kernel image up to its linked load address. Real-mode
    ; INT13h reads can't reach 1MB directly, so it was staged low at
    ; KERNEL_STAGE_SEG:0000 (physical 0x10000); now that we're in flat
    ; 32-bit protected mode this is a plain linear copy.
    mov esi, KERNEL_STAGE_SEG << 4
    mov edi, KERNEL_LOAD_ADDR
    mov ecx, (KERNEL_SECTORS * 512 + 3) / 4
    rep movsd

    ; Build struct multiboot_info (kernel/include/multiboot.h, 52 bytes,
    ; packed) at MB_INFO_ADDR. pmm_init() only reads flags/mmap_addr/
    ; mmap_length (plus each mmap entry's own fields) -- every other field
    ; is left zero, matching what an unused multiboot field means anyway.
    mov edi, MB_INFO_ADDR
    mov dword [edi + 0],  0x00000040     ; flags = MULTIBOOT_INFO_MMAP
    mov dword [edi + 4],  0              ; mem_lower (unused by pmm_init)
    mov dword [edi + 8],  0              ; mem_upper (unused)
    mov dword [edi + 12], 0              ; boot_device
    mov dword [edi + 16], 0              ; cmdline
    mov dword [edi + 20], 0              ; mods_count
    mov dword [edi + 24], 0              ; mods_addr
    mov dword [edi + 28], 0              ; syms[0]
    mov dword [edi + 32], 0              ; syms[1]
    mov dword [edi + 36], 0              ; syms[2]
    mov dword [edi + 40], 0              ; syms[3]
    mov eax, [mmap_count]
    imul eax, eax, 24
    mov dword [edi + 44], eax            ; mmap_length
    mov dword [edi + 48], MMAP_BUFFER    ; mmap_addr

    ; Hand off exactly like GRUB does: EAX = multiboot magic, EBX = info
    ; struct address, jump straight to _start (boot.s) at its real linked
    ; address -- it sets up its own stack and pushes eax/ebx for
    ; kernel_main itself, so no stack setup is needed here.
    mov eax, 0x2BADB002
    mov ebx, MB_INFO_ADDR
    jmp KERNEL_ENTRY
