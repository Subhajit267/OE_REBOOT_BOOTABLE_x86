/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: Kernel
File: kernel.c
About: Real OS kernel entry point (kernel.bin's `kernel_main`, called by
       boot/src/boot.s's _start with GRUB's multiboot magic/info -- or by
       the Stage-4 bootloader's synthetic equivalent). Brings up
       hal_initialize() -> pmm_init()/paging_init() -> disk+FAT mount
       (auto-formatting a genuinely blank/unformatted disk rather than
       halting) -> pal_init(), then hands off to the exact same hosted-app
       flow main.c uses (bootscreen -> login() or installer_prompt()) --
       pal/src/pal_kernel.c is the only thing that differs from a hosted
       Windows/Linux build.
Revisions:
- 2026-08-30  Initial creation (Phase 1): "Hello Kernel" boot only.
- 2026-08-30  Phase 2-6: interrupts, PMM/paging, HAL boundary, disk+FAT,
              pal_kernel.c brought up incrementally, demo code only.
- 2026-08-30  Phase 7: full hosted-app port -- kernel_main now runs the
              real flow (disk/FAT mount is fatal-if-both-mount-and-format
              fail, matching main.c line for line), demo functions removed.
------------------------------------------------------------
*/

#include "hal.h"
#include "pmm.h"
#include "paging.h"
#include "fat.h"
#include "mbr.h"
#include "pal.h"
#include "bootscreen.h"
#include "user.h"
#include "installer.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/*
   Low-level bring-up diagnostics go to serial only -- the real userland
   (bootscreen_show onward) owns the VGA screen from here on, the same way
   a hosted OS's boot log stays off the visible display once the login
   screen appears. A disk/FAT failure is the one thing that can't be
   silently swallowed: user.h's file-backed login has nothing to read or
   write without it.
*/
static void boot_diagnostics(unsigned long mb_info_addr)
{
    hal_debug_print("OE Reboot kernel alive.\n");

    pmm_init(mb_info_addr);
    paging_init();
    hal_debug_print("Memory manager + paging up.\n");

    if (!hal_disk_init())
    {
        hal_console_print("\033[31mFATAL: no disk found -- login/user storage has nowhere to live.\033[0m\n");
        hal_debug_print("FATAL: hal_disk_init() failed.\n");
        for (;;)
            __asm__ volatile ("hlt");
    }

    {
        unsigned int part_lba, part_sectors;
        int have_partition = mbr_find_partition(&part_lba, &part_sectors);
        int mounted_ok;

        if (have_partition)
        {
            hal_debug_print("MBR found a FAT partition -- mounting it, not the whole disk.\n");
            mounted_ok = fat_mount_at(part_lba);
        }
        else
        {
            hal_debug_print("No MBR/partition table found -- treating the whole disk as one volume.\n");
            mounted_ok = fat_mount();
        }

        if (!mounted_ok)
        {
            /* A genuinely blank disk (or partition) fails mount the same way
               a corrupt one would -- try formatting it before giving up. A
               disk that fails again after a real format attempt is an actual
               hardware/size problem, not just "unformatted", so that case
               still halts. */
            hal_debug_print("mount failed -- looks unformatted, formatting it.\n");
            hal_console_print("\033[33mNo filesystem found -- formatting disk...\033[0m\n");

            if (have_partition)
                mounted_ok = fat_format_at(part_lba, part_sectors) && fat_mount_at(part_lba);
            else
                mounted_ok = fat_format() && fat_mount();

            if (!mounted_ok)
            {
                hal_console_print("\033[31mFATAL: could not format disk (unusable size, or a write failure).\033[0m\n");
                hal_debug_print("FATAL: fat_format() or the post-format fat_mount() failed.\n");
                for (;;)
                    __asm__ volatile ("hlt");
            }
            hal_debug_print("Disk formatted.\n");
        }
    }
    hal_debug_print("FAT volume mounted.\n");
}

void kernel_main(unsigned long magic, unsigned long mb_info_addr)
{
    hal_initialize();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        hal_debug_print("Warning: not booted by a multiboot-compliant loader.\n");

    boot_diagnostics(mb_info_addr);

    /* From here on this mirrors main.c's real flow -- pal_kernel.c is the
       PAL backend, so nothing below differs from what pal_windows.c/
       pal_linux.c would run. */
    pal_init();
    pal_sleep(0.5);
    bootscreen_show(0);

    if (user_exists())
    {
        login();
    }
    else
    {
        /* Only shown ahead of first-time setup (no user.bd yet) -- a normal
           boot with an existing account goes straight to login() above with
           no extra message. */
        pal_println("Preparing for first boot...");
        pal_sleep(1);
        pal_clear_screen();
        installer_prompt();
    }

    for (;;)
        __asm__ volatile ("hlt");
}
