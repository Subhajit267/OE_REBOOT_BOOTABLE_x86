/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: ata.h
About: Primary-bus ATA PIO disk driver (polling, no IRQ14 wiring yet).
       ata_initialize() IDENTIFY-probes and bails on ATAPI rather than
       misreading it; ata_write_sector() flushes the drive's cache
       before returning so "success" really means "on disk". Backs
       kernel/src/fat.c's disk I/O via hal.h.
Revisions:
- 2026-08-30  Initial creation (Phase 5): IDENTIFY probe + read.
- 2026-08-30  Write support + cache flush added (Phase 7).
------------------------------------------------------------
*/

#ifndef HAL_ATA_H
#define HAL_ATA_H

#define ATA_SECTOR_SIZE 512

/* Probes the primary ATA bus's master drive via IDENTIFY. Returns 1 if a
   plain ATA (non-ATAPI) disk responded, 0 otherwise. Must succeed before
   ata_read_sector(). */
int ata_initialize(void);

/* Reads one 512-byte sector at 28-bit LBA `lba` into `buffer` via polled
   PIO -- no IRQ14 wiring yet (that's future work once something needs
   overlapped I/O instead of a busy-wait). Returns 1 on success, 0 on
   error or timeout. */
int ata_read_sector(unsigned int lba, void* buffer);

/* Writes one 512-byte sector, waits for the drive to finish, then flushes
   its write cache before returning -- so "returned success" really means
   "on disk", not just "queued". Returns 1 on success, 0 on error/timeout. */
int ata_write_sector(unsigned int lba, const void* buffer);

unsigned int ata_get_sector_count(void);

/* NUL-terminated, trailing-space-trimmed IDENTIFY model string (up to 40
   chars). Only valid after ata_initialize() succeeds. */
const char* ata_get_model_string(void);

#endif
