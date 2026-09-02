/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-31
Date Last Modified: 2026-08-31
Module: Kernel
File: mbr.c
About: Classic MBR partition table read/write. mbr_find_partition()
       reads LBA0, checks the 0x55AA signature, and scans the four
       primary entries for the first recognized FAT type byte --
       fat.c's fat_mount_at()/fat_format_at() then operate relative to
       that partition's start LBA instead of LBA 0. mbr_write() fills in
       exactly one partition entry (this kernel only ever creates one),
       optionally preserving existing bootstrap code (bytes 0-445) so
       writing the partition table never requires re-installing the
       boot loader that lives there.
Revisions:
- 2026-08-31  Initial creation (Stage 3 of the FAT32/MBR/bootloader/
              installer roadmap)
------------------------------------------------------------
*/

#include "mbr.h"
#include "hal.h"

#define MBR_PARTITION_TABLE_OFFSET 0x1BE /* 446 */
#define MBR_ENTRY_SIZE 16
#define MBR_ENTRY_COUNT 4

/* Standard 16-byte MBR partition-table entry layout. */
struct mbr_partition_entry
{
    unsigned char boot_flag;
    unsigned char chs_start[3];
    unsigned char type;
    unsigned char chs_end[3];
    unsigned int  lba_start;
    unsigned int  sector_count;
} __attribute__((packed));

static int is_fat_type(unsigned char type)
{
    return type == MBR_TYPE_FAT16 || type == MBR_TYPE_FAT16_LBA ||
           type == MBR_TYPE_FAT32_CHS || type == MBR_TYPE_FAT32_LBA;
}

int mbr_find_partition(unsigned int* out_start_lba, unsigned int* out_sector_count)
{
    unsigned char sector[HAL_DISK_SECTOR_SIZE];
    int i;

    if (!hal_disk_read_sector(0, sector))
        return 0;
    if (sector[510] != 0x55 || sector[511] != 0xAA)
        return 0; /* no MBR at all -- unpartitioned disk */

    for (i = 0; i < MBR_ENTRY_COUNT; i++)
    {
        struct mbr_partition_entry* e =
            (struct mbr_partition_entry*)(sector + MBR_PARTITION_TABLE_OFFSET + i * MBR_ENTRY_SIZE);

        if (e->type == 0x00)
            continue; /* unused entry */
        if (!is_fat_type(e->type))
            continue;
        if (e->lba_start == 0 || e->sector_count == 0)
            continue; /* malformed -- ignore rather than trust garbage */

        *out_start_lba    = e->lba_start;
        *out_sector_count = e->sector_count;
        return 1;
    }
    return 0;
}

int mbr_write(unsigned int partition_start_lba, unsigned int partition_sector_count,
              unsigned char partition_type, int preserve_bootstrap)
{
    unsigned char sector[HAL_DISK_SECTOR_SIZE];
    struct mbr_partition_entry* e;
    int i;

    if (preserve_bootstrap)
    {
        if (!hal_disk_read_sector(0, sector))
            return 0;
    }
    else
    {
        for (i = 0; i < HAL_DISK_SECTOR_SIZE; i++)
            sector[i] = 0;
    }

    /* This kernel only ever writes one partition -- clear the whole table
       (all 4 entries) regardless of preserve_bootstrap, then fill entry 1. */
    for (i = 0; i < MBR_ENTRY_COUNT * MBR_ENTRY_SIZE; i++)
        sector[MBR_PARTITION_TABLE_OFFSET + i] = 0;

    e = (struct mbr_partition_entry*)(sector + MBR_PARTITION_TABLE_OFFSET);
    e->boot_flag     = 0x80; /* active/bootable */
    e->chs_start[0]  = 0; e->chs_start[1] = 0; e->chs_start[2] = 0; /* CHS unused -- LBA only, cosmetic zero */
    e->type          = partition_type;
    e->chs_end[0]    = 0; e->chs_end[1] = 0; e->chs_end[2] = 0;
    e->lba_start     = partition_start_lba;
    e->sector_count  = partition_sector_count;

    sector[510] = 0x55;
    sector[511] = 0xAA;

    return hal_disk_write_sector(0, sector);
}
