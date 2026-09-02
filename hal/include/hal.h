/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-09-01
Module: HAL
File: hal.h
About: The hardware-abstraction boundary the rest of the kernel is meant
       to stay on this side of -- console, timer, keyboard, disk, and
       power control, backed by hal/src's arch-specific drivers (VGA,
       PIT, PS/2, ATA, ACPI). kernel/ code should only ever see this
       header, mirroring pal.h's "only pal_* touches libc" rule as
       "only hal/src's C files touch hardware directly."
Revisions:
- 2026-08-30  Initial creation (Phase 4: HAL boundary established --
              moved io_ports/vga/serial/gdt/idt/isr/irq/pic/pit/keyboard
              out of kernel/ into hal/, added this contract header).
- 2026-08-30  Disk (ata) and real ACPI-based power-off/reboot added
              (Phase 5, Phase 7).
- 2026-09-01  Added hal_keyboard_flush_raw()/hal_keyboard_flush_cooked()
              forwarding (see keyboard.h's revision note -- notepad
              garbage-text-on-open fix).
------------------------------------------------------------
*/

#ifndef HAL_H
#define HAL_H

/*
   The boundary the rest of the kernel is meant to stay on this side of.
   Everything arch-specific -- VGA text mode, the 8259 PICs, PIT, PS/2
   keyboard, GDT/IDT, port I/O -- lives in hal/src and is not declared here;
   kernel/ code should only ever see this header, the same way pal.h is the
   only thing the hosted userland modules are allowed to call through
   (pal/include/pal.h's rule that only pal_* touches libc, mirrored here as
   only hal/src's C files touching hardware directly). A second architecture
   would mean a second hal/src backend implementing this same contract, not
   a rewrite of kernel/.
*/

/* Brings up GDT, IDT, CPU exception handlers, IRQ dispatch, PIC remap,
   the PIT (see HAL_TIMER_HZ), and the keyboard driver, then enables
   interrupts (sti). Call once, early in kernel_main. */
void hal_initialize(void);

#define HAL_TIMER_HZ 100

void hal_console_print(const char* str);
void hal_console_putchar(char c);
void hal_console_get_size(int* rows, int* cols);
void hal_debug_print(const char* str); /* serial -- the only output before there's a GUI, and still the boot-verification channel */

unsigned long hal_timer_get_ticks(void);

/* Cooked queue: mapped ASCII only, key releases and unmapped keys already
   dropped. */
int hal_keyboard_read_char(void);     /* blocks until a char is available */
int hal_keyboard_try_read_char(void); /* -1 if none is waiting */

/* Raw queue: every make-code byte, undecoded, plus its 0xE0-prefix flag
   and the same shift-aware ASCII translation the cooked queue uses (0 if
   the key has none). Mapping the scancode onto an application-level key
   code for extended keys is deliberately left to the caller (pal_kernel.c's
   PAL_SC_xxx), not done here, since that numbering is PAL's contract, not
   the machine's. */
int hal_keyboard_try_read_raw(unsigned char* scancode, int* extended, char* ascii);

/* The cooked and raw queues above are both fed from every keystroke
   unconditionally, so whichever one isn't being drained silently
   accumulates a backlog -- call these at a cooked/raw mode transition to
   discard it instead of having the other mode replay it later. */
void hal_keyboard_flush_raw(void);
void hal_keyboard_flush_cooked(void);

#define HAL_DISK_SECTOR_SIZE 512

/* Probes the primary ATA disk. Unlike hal_initialize()'s components, a disk
   is genuinely optional hardware -- returns 1 if one responded, 0 if not,
   and callers (fat.c's mount) are expected to handle "no disk" rather than
   this hiding the failure. */
int hal_disk_init(void);

int hal_disk_read_sector(unsigned int lba, void* buffer);
int hal_disk_write_sector(unsigned int lba, const void* buffer);
unsigned int hal_disk_get_sector_count(void);

/* NUL-terminated, trailing-space-trimmed disk model string. Only valid
   after hal_disk_init() succeeds. */
const char* hal_disk_get_model_string(void);

/* Real ACPI power-off. Tries the two legacy Bochs/QEMU debug shutdown
   ports first (0x604 and 0xB004 -- near-free, covers both older and newer
   QEMU machine types), then falls back to a genuine ACPI S5 soft-off
   (hal/src/acpi.c: parses the real RSDP/RSDT-or-XSDT/FADT/DSDT chain and
   writes the \_S5 SLP_TYP/SLP_EN bits to PM1a_CNT_BLK) -- this second path
   is what actually powers off VirtualBox and real hardware, neither of
   which answer the QEMU/Bochs ports. Falls through to hlt only if no valid
   ACPI chain can be found at all. Never returns. */
void hal_power_off(void);

/* Real warm reboot via the 8042 keyboard controller's reset line -- the
   one method real BIOS-era PCs, QEMU, and VirtualBox all support (unlike
   the ACPI ports above). Falls back to forcing a triple fault (invalid IDT)
   if the 8042 pulse doesn't take. Never returns. */
void hal_reboot(void);

#endif
