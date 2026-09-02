/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-31
Date Last Modified: 2026-09-01
Module: HAL
File: acpi.h
About: Real ACPI S5 ("soft off") power-off -- locates the RSDP -> RSDT/
       XSDT -> FADT -> DSDT chain from scratch and writes the \_S5
       package's SLP_TYP/SLP_EN bits to PM1a_CNT_BLK. What real hardware
       and VirtualBox need for a genuine power-off (QEMU/Bochs also
       expose a debug-port shortcut, tried first in hal_power_off()).
Revisions:
- 2026-08-31  Initial creation.
- 2026-09-01  Every physical-address dereference in the table chain now
              bounds-checked against paging.h's identity-map limit
              before touching it (see acpi.c) -- fixes a page fault on
              VMs with RAM above the map's old cap, where SeaBIOS had
              placed the real tables near the top of RAM.
------------------------------------------------------------
*/

#ifndef HAL_ACPI_H
#define HAL_ACPI_H

/* Real ACPI S5 ("soft off") power-off: locates the RSDP -> RSDT/XSDT ->
   FADT -> DSDT chain from scratch (BIOS tables, not anything the kernel
   already knows about), extracts the \_S5 package's SLP_TYPa/SLP_TYPb
   values via a targeted byte-pattern scan of the DSDT's AML (the standard
   simplified approach -- a full AML interpreter is not needed just to read
   two small integers out of one well-known package), and writes the
   SLP_TYP + SLP_EN bits to PM1a_CNT_BLK (and PM1b_CNT_BLK if the machine
   has one). This is what real hardware and VirtualBox both need for a
   genuine power-off; QEMU/Bochs additionally expose a shortcut debug port
   (handled separately in hal_power_off()) but real ACPI is what makes this
   work everywhere else too.

   Returns 0 immediately, touching no hardware, if a valid RSDP/table chain
   can't be found and checksum-validated -- callers should fall back to
   something else rather than trust a partially-parsed chain. On success
   this writes the shutdown command and does not return (the machine powers
   off); if a real ACPI implementation somehow ignores the write, it still
   returns 1 so the caller knows the attempt was well-formed. */
int acpi_power_off(void);

#endif
