/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-31
Module: HAL
File: ata.c
About: Primary-bus ATA PIO disk driver, polling only. ata_initialize()
       IDENTIFY-probes and bails on ATAPI rather than misreading it;
       ata_read_sector()'s timeout is wall-clock via pit_get_ticks() (not
       a raw spin count), so it means the same thing regardless of CPU
       speed. ata_write_sector() flushes the drive's write cache before
       returning so "success" really means "on disk", not just "queued".
Revisions:
- 2026-08-30  Initial creation (Phase 5): IDENTIFY probe + read.
- 2026-08-30  Write support + cache flush added (Phase 7).
- 2026-08-31  Added ata_get_model_string() extraction from IDENTIFY
              words 27-46 (byte-swapped per word, a well-known ATA
              quirk) so the disk installer can show the real target
              disk's model instead of a placeholder.
------------------------------------------------------------
*/

#include "ata.h"
#include "io_ports.h"
#include "pit.h"

/* Primary ATA bus, I/O ports (control block port 0x3F6 isn't used -- this
   driver doesn't touch nIEN/SRST, only the polled command/status path). */
#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7

#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_BSY  0x80

#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY      0xEC
#define ATA_CMD_CACHE_FLUSH   0xE7

#define ATA_TIMEOUT_TICKS 500 /* ~5s at HAL_TIMER_HZ=100 -- generous for PIO, still bounded */

static unsigned int sector_count;
static char model_string[41];

/* Bounded on PIT ticks (already running by the time this driver is used)
   rather than a raw spin count, so the timeout means the same thing
   regardless of CPU speed. Returns 1 if `mask` bits went the way wanted
   before timing out, 0 on timeout. */
static int wait_status(unsigned char mask, unsigned char want)
{
    unsigned long start = pit_get_ticks();

    for (;;)
    {
        unsigned char status = inb(ATA_STATUS);
        if ((status & mask) == want)
            return 1;
        if (status & ATA_STATUS_ERR)
            return 0;
        if (pit_get_ticks() - start > ATA_TIMEOUT_TICKS)
            return 0;
    }
}

int ata_initialize(void)
{
    int i;
    unsigned short identify[256];

    outb(ATA_DRIVE_HEAD, 0xA0); /* select master, no LBA bits needed for IDENTIFY */
    outb(ATA_SECCOUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    if (inb(ATA_STATUS) == 0)
        return 0; /* no drive on this bus */

    if (!wait_status(ATA_STATUS_BSY, 0))
        return 0;

    /* Per spec, a non-ATA device (e.g. ATAPI) leaves LBAmid/LBAhigh nonzero
       here instead of proceeding straight to DRQ -- bail rather than
       misreading its IDENTIFY data as if it were ours. */
    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HIGH) != 0)
        return 0;

    if (!wait_status(ATA_STATUS_DRQ, ATA_STATUS_DRQ))
        return 0;

    for (i = 0; i < 256; i++)
        identify[i] = inw(ATA_DATA);

    sector_count = (unsigned int)identify[60] | ((unsigned int)identify[61] << 16);

    /* Model string: IDENTIFY words 27-46 (40 bytes), each word's two
       bytes stored high-byte-first -- opposite of every other field in
       this struct, a well-known ATA quirk. Space-padded, not
       NUL-terminated by the device, so trim trailing spaces ourselves. */
    for (i = 0; i < 20; i++)
    {
        unsigned short w = identify[27 + i];
        model_string[i * 2]     = (char)((w >> 8) & 0xFF);
        model_string[i * 2 + 1] = (char)(w & 0xFF);
    }
    model_string[40] = '\0';
    for (i = 39; i >= 0 && model_string[i] == ' '; i--)
        model_string[i] = '\0';

    return 1;
}

int ata_read_sector(unsigned int lba, void* buffer)
{
    unsigned short* buf16 = (unsigned short*)buffer;
    int i;

    outb(ATA_DRIVE_HEAD, (unsigned char)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW, (unsigned char)(lba & 0xFF));
    outb(ATA_LBA_MID, (unsigned char)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (unsigned char)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);

    if (!wait_status(ATA_STATUS_BSY | ATA_STATUS_DRQ, ATA_STATUS_DRQ))
        return 0;

    for (i = 0; i < 256; i++)
        buf16[i] = inw(ATA_DATA);

    return 1;
}

int ata_write_sector(unsigned int lba, const void* buffer)
{
    const unsigned short* buf16 = (const unsigned short*)buffer;
    int i;

    outb(ATA_DRIVE_HEAD, (unsigned char)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW, (unsigned char)(lba & 0xFF));
    outb(ATA_LBA_MID, (unsigned char)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (unsigned char)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, ATA_CMD_WRITE_SECTORS);

    if (!wait_status(ATA_STATUS_BSY | ATA_STATUS_DRQ, ATA_STATUS_DRQ))
        return 0;

    for (i = 0; i < 256; i++)
        outw(ATA_DATA, buf16[i]);

    if (!wait_status(ATA_STATUS_BSY, 0))
        return 0;

    /* Flush the drive's write cache before returning "done" -- without
       this, a reset right after a write could lose data QEMU/real
       hardware still had buffered. */
    outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
    return wait_status(ATA_STATUS_BSY, 0);
}

unsigned int ata_get_sector_count(void)
{
    return sector_count;
}

const char* ata_get_model_string(void)
{
    return model_string;
}
