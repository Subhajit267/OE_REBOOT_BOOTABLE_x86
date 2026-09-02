/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-09-01
Module: HAL
File: hal.c
About: Implements hal.h's contract by forwarding to the individual
       arch-specific drivers (VGA, serial, GDT/IDT/ISR/IRQ/PIC/PIT,
       keyboard, ATA, ACPI) -- the one file kernel/ code's calls
       actually resolve into. hal_power_off()/hal_reboot() layer
       multiple fallbacks so both never return: QEMU/Bochs debug ports
       -> real ACPI S5 -> bare hlt for power-off; 8042 reset pulse ->
       forced triple fault for reboot.
Revisions:
- 2026-08-30  Initial creation (Phase 4: HAL boundary established).
- 2026-08-30  Disk (ata) and real ACPI-based power-off/reboot added
              (Phase 5, Phase 7).
- 2026-09-01  Added hal_keyboard_flush_raw()/hal_keyboard_flush_cooked()
              forwarding (see keyboard.c's revision note).
------------------------------------------------------------
*/

#include "hal.h"
#include "vga.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "irq.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "ata.h"
#include "io_ports.h"
#include "acpi.h"

void hal_initialize(void)
{
    vga_initialize();
    serial_initialize();

    gdt_initialize();
    idt_initialize();
    isr_install();
    irq_install();
    pic_remap(IRQ_BASE, IRQ_BASE + 8);
    pit_initialize(HAL_TIMER_HZ);
    keyboard_initialize();

    __asm__ volatile ("sti");
}

void hal_console_print(const char* str)
{
    vga_print(str);
}

void hal_console_putchar(char c)
{
    vga_putchar(c);
}

void hal_console_get_size(int* rows, int* cols)
{
    vga_get_size(rows, cols);
}

void hal_debug_print(const char* str)
{
    serial_print(str);
}

unsigned long hal_timer_get_ticks(void)
{
    return pit_get_ticks();
}

int hal_keyboard_read_char(void)
{
    return keyboard_read_char();
}

int hal_keyboard_try_read_char(void)
{
    return keyboard_try_read_char();
}

int hal_keyboard_try_read_raw(unsigned char* scancode, int* extended, char* ascii)
{
    return keyboard_try_read_raw(scancode, extended, ascii);
}

void hal_keyboard_flush_raw(void)
{
    keyboard_flush_raw();
}

void hal_keyboard_flush_cooked(void)
{
    keyboard_flush_cooked();
}

int hal_disk_init(void)
{
    return ata_initialize();
}

int hal_disk_read_sector(unsigned int lba, void* buffer)
{
    return ata_read_sector(lba, buffer);
}

int hal_disk_write_sector(unsigned int lba, const void* buffer)
{
    return ata_write_sector(lba, buffer);
}

unsigned int hal_disk_get_sector_count(void)
{
    return ata_get_sector_count();
}

const char* hal_disk_get_model_string(void)
{
    return ata_get_model_string();
}

void hal_power_off(void)
{
    /* Newer QEMU machine types (Q35/i440fx post ~2016) wire the PIIX4/ICH9
       ACPI PM1a_CNT-equivalent debug shutdown at 0x604; older QEMU/Bochs
       used 0xB004. Try both first -- near-free, and harmless no-ops on
       hardware/hypervisors (like VirtualBox) that don't implement either. */
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);

    /* VirtualBox and real hardware don't answer to the ports above, but do
       implement real ACPI -- parse the actual RSDP/FADT/DSDT chain and
       issue a genuine S5 soft-off. If a valid chain can't be found (should
       not happen on any real ACPI-era machine, but this must never hang
       without at least reaching hlt), fall through below. */
    acpi_power_off();

    for (;;)
        __asm__ volatile ("hlt");
}

static void wait_8042_input_clear(void)
{
    /* Bit 1 of the 8042 status port: 1 while the input buffer still holds
       a byte the controller hasn't consumed yet. Must be 0 before writing
       another command byte, or the write can be silently dropped. */
    int spins;
    for (spins = 0; spins < 100000 && (inb(0x64) & 0x02); spins++)
        ;
}

void hal_reboot(void)
{
    wait_8042_input_clear();
    outb(0x64, 0xFE); /* 8042 CPU-reset pulse -- universally supported: real hardware, QEMU, and VirtualBox all implement this legacy line. */

    /* Fallback if the controller didn't reset us (shouldn't happen on any
       target this project runs on, but never returning matters more than
       which mechanism did it): load a zero-limit IDT so the very next
       interrupt has nowhere valid to vector to, then force one -- the CPU
       triple-faults and the real hardware/hypervisor resets it. */
    {
        struct { unsigned short limit; unsigned int base; } __attribute__((packed)) bad_idt = { 0, 0 };
        __asm__ volatile ("lidt %0" : : "m"(bad_idt));
        __asm__ volatile ("int $0x03");
    }

    for (;;)
        __asm__ volatile ("hlt");
}
