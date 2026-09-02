/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-31
Date Last Modified: 2026-08-31
Module: Kernel
File: mbr.h
About: Classic MBR partition table read/write -- locates the first FAT-
       type partition entry (mbr_find_partition()) so fat.c can mount a
       real partition instead of treating the whole disk as one
       unpartitioned "superfloppy" volume, and writes a single partition
       entry (mbr_write()) for the disk installer (installer/src/
       installer_main.c) to declare the OEFS partition it just formatted.
Revisions:
- 2026-08-31  Initial creation (Stage 3 of the FAT32/MBR/bootloader/
              installer roadmap)
------------------------------------------------------------
*/

#ifndef KERNEL_MBR_H
#define KERNEL_MBR_H

/* Classic MBR partition type bytes this kernel recognizes as FAT. Real
   installers/other OSes use both the CHS-era and LBA-era type bytes for the
   same filesystem, so both are checked. */
#define MBR_TYPE_FAT16      0x06
#define MBR_TYPE_FAT16_LBA  0x0E
#define MBR_TYPE_FAT32_CHS  0x0B
#define MBR_TYPE_FAT32_LBA  0x0C

/* Reads LBA 0, checks the 0x55AA boot signature, and scans the four primary
   partition-table entries (offset 0x1BE) for the first one whose type byte
   is a recognized FAT type. Returns 1 with `out_start_lba`/`out_sector_count`
   filled in on a match, 0 if there's no valid MBR signature or no matching
   entry -- callers should fall back to treating the whole disk as one
   unpartitioned "superfloppy" volume (fat_mount()/fat_format() at LBA 0),
   which is what every disk this project has used until now already is. */
int mbr_find_partition(unsigned int* out_start_lba, unsigned int* out_sector_count);

/* Writes partition table entry 1 (of 4) describing a single FAT partition:
   `partition_start_lba` for `partition_sector_count` sectors, marked
   active/bootable, with type byte `partition_type`. Entries 2-4 are always
   zeroed -- this kernel only ever creates one partition. If
   `preserve_bootstrap` is nonzero, the existing bytes at offset 0-445 (where
   a boot sector's bootstrap code lives) are read back and kept; otherwise
   they're zeroed. Returns 1 on success, 0 on a disk I/O failure. */
int mbr_write(unsigned int partition_start_lba, unsigned int partition_sector_count,
              unsigned char partition_type, int preserve_bootstrap);

#endif
