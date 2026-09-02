/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-31
Module: Kernel
File: fat.c
About: FAT12/16/32 driver implementation. Parses the BPB from LBA0 and
       decides FAT12 vs FAT16 vs FAT32 by Microsoft's own cluster-count
       test (fatgen103), not by trusting the boot sector's label string.
       fat_format_at() writes a fresh volume from scratch (boot sector,
       both FAT copies, FSInfo for FAT32, zeroed root) -- the real
       "mkfs" a disk installer needs against a genuinely blank disk.
       struct fat_dir's fixed-region-vs-cluster-chain split, plus
       for_each_dirent_in_dir()/fat_resolve_path(), is what makes real
       subdirectories (not just a flat root) possible on any FAT type.
Revisions:
- 2026-08-30  Initial creation (Phase 5): FAT12/16 mount + read only,
              root-directory-only.
- 2026-08-30  Write support added (Phase 7): fat_write_file/delete_file,
              writes propagate to every FAT copy on disk, not just #0.
- 2026-08-31  Rewritten for FAT32 (fatgen103 cluster-size table, FSInfo,
              4-byte FAT entries) and real subdirectories (path-aware
              `_in()` API built on a generalized cluster-chain walker,
              real '.'/'..' dirents). Volume OEM string/label branded
              "OEFS" -- still byte-for-byte standard FAT, independently
              mtools-readable.
------------------------------------------------------------
*/

#include "fat.h"
#include "hal.h"

struct fat_bpb
{
    unsigned char  jmp[3];
    char           oem[8];
    unsigned short bytes_per_sector;
    unsigned char  sectors_per_cluster;
    unsigned short reserved_sectors;
    unsigned char  num_fats;
    unsigned short root_entry_count;
    unsigned short total_sectors_16;
    unsigned char  media_type;
    unsigned short fat_size_16;
    unsigned short sectors_per_track;
    unsigned short num_heads;
    unsigned int   hidden_sectors;
    unsigned int   total_sectors_32;
} __attribute__((packed));

/* Extended BPB for FAT32, mapped at byte offset 36 (right after
   total_sectors_32) -- fat_size_16 is 0 on a FAT32 volume and this
   structure's fat_size_32 is authoritative instead. */
struct fat32_ext_bpb
{
    unsigned int   fat_size_32;
    unsigned short ext_flags;
    unsigned short fs_version;
    unsigned int   root_cluster;
    unsigned short fs_info_sector;
    unsigned short backup_boot_sector;
    unsigned char  reserved[12];
    unsigned char  drive_number;
    unsigned char  reserved1;
    unsigned char  boot_sig;
    unsigned int   volume_id;
    char           volume_label[11];
    char           fs_type[8];
} __attribute__((packed));

/* Extended BPB for FAT12/16, same offset (36) as fat32_ext_bpb but a
   different, shorter layout -- which one applies is decided by fat_size_16
   (0 means FAT32). */
struct fat1x_ext_bpb
{
    unsigned char  drive_number;
    unsigned char  reserved1;
    unsigned char  boot_sig;
    unsigned int   volume_id;
    char           volume_label[11];
    char           fs_type[8];
} __attribute__((packed));

struct fat_raw_dirent
{
    unsigned char  name[8];
    unsigned char  ext[3];
    unsigned char  attr;
    unsigned char  reserved1[8];      /* NT case flags / create-time fields -- unused */
    unsigned short first_cluster_hi;  /* FAT32 only; ignored (must stay 0) for FAT12/16 */
    unsigned short time;
    unsigned short date;
    unsigned short first_cluster;     /* low 16 bits for FAT32, full value for FAT12/16 */
    unsigned int   size;
} __attribute__((packed));

#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  FAT_ATTR_DIRECTORY
#define ATTR_LONG_NAME  0x0F

enum { FAT12, FAT16, FAT32 };

static unsigned int bytes_per_sector;
static unsigned int sectors_per_cluster;
static unsigned int fat_start_lba;
static unsigned int num_fats;
static unsigned int fat_size_sectors;
static unsigned int root_dir_start_lba;
static unsigned int root_dir_sectors;
static unsigned int root_entry_count;
static unsigned int root_cluster; /* FAT32 only */
static unsigned int data_start_lba;
static unsigned int total_clusters;
static int fat_type;
static int mounted;
static struct fat_dir root_dir;

/* LBA of the start of this volume on the physical disk -- 0 for the
   unpartitioned "superfloppy" volumes this driver has always used
   (fat_mount()/fat_format()), or a real partition's start LBA when mounted
   via fat_mount_at()/fat_format_at() after mbr_find_partition() locates one.
   Every LBA this driver computes elsewhere in the file is relative to this
   base -- disk_read()/disk_write() are the only two places the base is
   actually added, so nothing above them needs to know partitions exist. */
static unsigned int partition_base_lba = 0;

static int disk_read(unsigned int lba, void* buf)
{
    return hal_disk_read_sector(partition_base_lba + lba, buf);
}

static int disk_write(unsigned int lba, const void* buf)
{
    return hal_disk_write_sector(partition_base_lba + lba, buf);
}

static unsigned int fat_cached_sector = 0xFFFFFFFFu;
static unsigned char fat_cache[HAL_DISK_SECTOR_SIZE];

/* Tiny local string-equality check -- fat.c is below the PAL layer and
   can't call pal_strcmp() (that would be a layering violation the other
   way around), so path-component comparisons need their own helper. */
static int str_eq(const char* a, const char* b)
{
    int i;
    for (i = 0; a[i] != '\0' || b[i] != '\0'; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

static unsigned int dirent_get_cluster(const struct fat_raw_dirent* e)
{
    if (fat_type == FAT32)
        return ((unsigned int)e->first_cluster_hi << 16) | e->first_cluster;
    return e->first_cluster;
}

static void dirent_set_cluster(struct fat_raw_dirent* e, unsigned int cluster)
{
    e->first_cluster = (unsigned short)(cluster & 0xFFFFu);
    e->first_cluster_hi = (fat_type == FAT32) ? (unsigned short)((cluster >> 16) & 0xFFFFu) : 0;
}

/* True if `dir` is the volume's root directory -- needed because a new
   subdirectory's '..' entry must store cluster 0 when its parent is root
   (real FAT convention, followed even by FAT32 where root has a genuine
   cluster number), not root's actual cluster/region. */
static int dir_is_root(const struct fat_dir* dir)
{
    if (root_dir.is_fixed)
        return dir->is_fixed != 0; /* FAT12/16: the only fixed-region directory that ever exists is root */
    return !dir->is_fixed && dir->start_cluster == root_dir.start_cluster;
}

/* `base_lba` is which copy of the FAT to hit -- fat_start_lba for reads
   (copy 0 is authoritative for anything this kernel reads back), or
   fat_start_lba + copy*fat_size_sectors when set_fat_entry() is writing
   every copy in step. */
static unsigned char fat_read_byte_at(unsigned int base_lba, unsigned int byte_offset)
{
    unsigned int sector = base_lba + byte_offset / bytes_per_sector;
    unsigned int offset = byte_offset % bytes_per_sector;

    if (sector != fat_cached_sector)
    {
        if (!disk_read(sector, fat_cache))
            return 0xFF; /* read failure: return an implausible byte rather than garbage state */
        fat_cached_sector = sector;
    }
    return fat_cache[offset];
}

static int fat_write_byte_at(unsigned int base_lba, unsigned int byte_offset, unsigned char value)
{
    unsigned int sector = base_lba + byte_offset / bytes_per_sector;
    unsigned int offset = byte_offset % bytes_per_sector;
    unsigned char buf[HAL_DISK_SECTOR_SIZE];

    if (!disk_read(sector, buf))
        return 0;
    buf[offset] = value;
    if (!disk_write(sector, buf))
        return 0;

    if (sector == fat_cached_sector)
        fat_cache[offset] = value; /* keep the read cache coherent instead of invalidating it */
    return 1;
}

static unsigned char fat_read_byte(unsigned int byte_offset)
{
    return fat_read_byte_at(fat_start_lba, byte_offset);
}

static unsigned int get_fat_entry(unsigned int cluster)
{
    if (fat_type == FAT32)
    {
        unsigned int off = cluster * 4;
        unsigned int b0 = fat_read_byte(off);
        unsigned int b1 = fat_read_byte(off + 1);
        unsigned int b2 = fat_read_byte(off + 2);
        unsigned int b3 = fat_read_byte(off + 3);
        return (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)) & 0x0FFFFFFFu; /* top 4 bits reserved */
    }
    else if (fat_type == FAT16)
    {
        unsigned int off = cluster * 2;
        unsigned int lo = fat_read_byte(off);
        unsigned int hi = fat_read_byte(off + 1);
        return lo | (hi << 8);
    }
    else /* FAT12 */
    {
        unsigned int off = cluster + cluster / 2;
        unsigned int lo = fat_read_byte(off);
        unsigned int hi = fat_read_byte(off + 1);
        unsigned int val = lo | (hi << 8);
        return (cluster & 1) ? (val >> 4) : (val & 0x0FFF);
    }
}

/* Writes one FAT entry to every copy on disk (num_fats copies -- real FAT
   volumes keep more than one as a backup; leaving copy 1+ stale would be
   a latent corruption bug for any other tool that ever reads this disk). */
static int set_fat_entry(unsigned int cluster, unsigned int value)
{
    unsigned int copy;

    for (copy = 0; copy < num_fats; copy++)
    {
        unsigned int base = fat_start_lba + copy * fat_size_sectors;

        if (fat_type == FAT32)
        {
            unsigned int off = cluster * 4;
            unsigned int preserved_top = fat_read_byte_at(base, off + 3) & 0xF0u; /* top 4 bits reserved -- preserve, don't clobber */

            if (!fat_write_byte_at(base, off, (unsigned char)(value & 0xFF)))
                return 0;
            if (!fat_write_byte_at(base, off + 1, (unsigned char)((value >> 8) & 0xFF)))
                return 0;
            if (!fat_write_byte_at(base, off + 2, (unsigned char)((value >> 16) & 0xFF)))
                return 0;
            if (!fat_write_byte_at(base, off + 3, (unsigned char)(((value >> 24) & 0x0Fu) | preserved_top)))
                return 0;
        }
        else if (fat_type == FAT16)
        {
            unsigned int off = cluster * 2;
            if (!fat_write_byte_at(base, off, (unsigned char)(value & 0xFF)))
                return 0;
            if (!fat_write_byte_at(base, off + 1, (unsigned char)((value >> 8) & 0xFF)))
                return 0;
        }
        else /* FAT12: entries share nibbles across a byte, so read-modify-write */
        {
            unsigned int off = cluster + cluster / 2;
            unsigned int lo = fat_read_byte_at(base, off);
            unsigned int hi = fat_read_byte_at(base, off + 1);
            unsigned int existing = lo | (hi << 8);
            unsigned int merged = (cluster & 1)
                ? (existing & 0x000F) | ((value & 0x0FFF) << 4)
                : (existing & 0xF000) | (value & 0x0FFF);

            if (!fat_write_byte_at(base, off, (unsigned char)(merged & 0xFF)))
                return 0;
            if (!fat_write_byte_at(base, off + 1, (unsigned char)((merged >> 8) & 0xFF)))
                return 0;
        }
    }
    return 1;
}

static int is_end_of_chain(unsigned int entry)
{
    if (fat_type == FAT32)
        return entry >= 0x0FFFFFF8u;
    return fat_type == FAT16 ? entry >= 0xFFF8 : entry >= 0xFF8;
}

/* Scans the FAT for a free (0) entry and immediately marks it end-of-chain
   so a second allocation call can't hand out the same cluster before the
   first caller links it in. Returns 0 (never a valid data cluster) on a
   full disk. */
static unsigned int alloc_cluster(void)
{
    unsigned int cluster;

    for (cluster = 2; cluster < total_clusters + 2; cluster++)
    {
        if (get_fat_entry(cluster) == 0)
        {
            unsigned int eoc = (fat_type == FAT32) ? 0x0FFFFFFFu : (fat_type == FAT16) ? 0xFFFFu : 0xFFFu;
            if (!set_fat_entry(cluster, eoc))
                return 0;
            return cluster;
        }
    }
    return 0;
}

static void free_chain(unsigned int cluster)
{
    while (cluster >= 2 && !is_end_of_chain(cluster))
    {
        unsigned int next = get_fat_entry(cluster);
        set_fat_entry(cluster, 0);
        cluster = next;
    }
}

int fat_mount_at(unsigned int partition_start_lba)
{
    unsigned char sector[HAL_DISK_SECTOR_SIZE];
    struct fat_bpb* bpb;
    unsigned int total_sectors, data_sectors;

    mounted = 0;
    fat_cached_sector = 0xFFFFFFFFu;
    partition_base_lba = partition_start_lba;

    if (!disk_read(0, sector))
        return 0;

    if (sector[510] != 0x55 || sector[511] != 0xAA)
        return 0; /* no valid boot signature */

    bpb = (struct fat_bpb*)sector;

    bytes_per_sector    = bpb->bytes_per_sector;
    sectors_per_cluster = bpb->sectors_per_cluster;
    root_entry_count    = bpb->root_entry_count;

    if (bytes_per_sector == 0 || sectors_per_cluster == 0 || bpb->num_fats == 0)
        return 0;

    fat_start_lba = bpb->reserved_sectors;
    num_fats      = bpb->num_fats;
    total_sectors = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;

    if (bpb->fat_size_16 != 0)
    {
        /* fat_size_16 nonzero means FAT12/16: fixed-region root directory. */
        fat_size_sectors   = bpb->fat_size_16;
        root_dir_start_lba = fat_start_lba + num_fats * fat_size_sectors;
        root_dir_sectors   = ((root_entry_count * 32u) + (bytes_per_sector - 1)) / bytes_per_sector;
        data_start_lba     = root_dir_start_lba + root_dir_sectors;

        if (total_sectors <= data_start_lba)
            return 0;

        data_sectors   = total_sectors - data_start_lba;
        total_clusters = data_sectors / sectors_per_cluster;

        /* Microsoft's own FAT-type test: cluster count alone decides
           FAT12 vs FAT16, not any label string in the boot sector. A
           cluster count landing in FAT32 range here would mean the BPB is
           internally inconsistent (fat_size_16 nonzero but too big a
           volume) -- reject rather than misread it. */
        if (total_clusters < 4085)
            fat_type = FAT12;
        else if (total_clusters < 65525)
            fat_type = FAT16;
        else
            return 0;

        root_dir.is_fixed      = 1;
        root_dir.fixed_lba     = root_dir_start_lba;
        root_dir.fixed_sectors = root_dir_sectors;
        root_dir.start_cluster = 0;
    }
    else
    {
        /* fat_size_16 == 0 signals FAT32 -- fat_size_32 (extended BPB,
           offset 36) is authoritative instead, and the root directory is a
           normal cluster chain rather than a fixed region. */
        struct fat32_ext_bpb* ext = (struct fat32_ext_bpb*)(sector + 36);

        fat_size_sectors = ext->fat_size_32;
        if (fat_size_sectors == 0)
            return 0;

        root_dir_start_lba = 0;
        root_dir_sectors   = 0;
        data_start_lba     = fat_start_lba + num_fats * fat_size_sectors;

        if (total_sectors <= data_start_lba)
            return 0;

        data_sectors   = total_sectors - data_start_lba;
        total_clusters = data_sectors / sectors_per_cluster;

        if (total_clusters < 65525)
            return 0; /* fat_size_16==0 but not enough clusters for real FAT32 -- malformed */

        fat_type     = FAT32;
        root_cluster = ext->root_cluster ? ext->root_cluster : 2;

        root_dir.is_fixed      = 0;
        root_dir.fixed_lba     = 0;
        root_dir.fixed_sectors = 0;
        root_dir.start_cluster = root_cluster;
    }

    mounted = 1;
    return 1;
}

int fat_mount(void)
{
    return fat_mount_at(0);
}

/*
   Writes a fresh, empty volume: boot sector + BPB, both FAT copies (entries
   0/1 -- and 2, for FAT32's single-cluster root -- set to their
   spec-reserved values, everything else zero), and a zeroed root directory.
   This is what a real installer's "format the disk" step does -- without
   it, this kernel could only ever boot against a disk some other tool had
   already formatted, never a genuinely blank one.

   Picks FAT12/16 (fixed-size root, Microsoft's fatgen103 cluster-size
   table up to ~2GB) or FAT32 (cluster-chain root -- cluster 2, right after
   the FAT -- fatgen103's larger-volume table) automatically by disk size.
   The on-disk OEM string and volume label are branded "OEFS" -- this is
   still a byte-for-byte standard FAT volume of whichever type, readable by
   any other FAT driver; "OEFS" is just this project's name for it.
   Returns 1 on success, 0 if the disk is unusable.
*/
int fat_format_at(unsigned int partition_start_lba, unsigned int partition_sector_count)
{
    unsigned int total_sectors = partition_sector_count;
    unsigned int reserved_sectors;
    unsigned int root_entries;
    unsigned int root_sectors;
    unsigned int num_fat_copies = 2;
    unsigned int spc, fat_sectors, iter;
    int type;
    unsigned char boot_sector[HAL_DISK_SECTOR_SIZE];
    unsigned char zero_sector[HAL_DISK_SECTOR_SIZE];
    unsigned char fat0_sector[HAL_DISK_SECTOR_SIZE];
    struct fat_bpb* bpb;
    unsigned int i, copy, s;

    partition_base_lba = partition_start_lba;

    if (total_sectors == 0)
        return 0;

    if (total_sectors <= 4194304) /* <=2GB: FAT12/16, exactly as before */
    {
        reserved_sectors = 1;
        root_entries     = 512;
        root_sectors     = (root_entries * 32u + HAL_DISK_SECTOR_SIZE - 1) / HAL_DISK_SECTOR_SIZE;

             if (total_sectors <= 65536)   spc = 1;   /* <=32MB  */
        else if (total_sectors <= 131072)  spc = 2;   /* <=64MB  */
        else if (total_sectors <= 262144)  spc = 4;   /* <=128MB */
        else if (total_sectors <= 524288)  spc = 8;   /* <=256MB */
        else if (total_sectors <= 1048576) spc = 16;  /* <=512MB */
        else if (total_sectors <= 2097152) spc = 32;  /* <=1GB   */
        else                                spc = 64;  /* <=2GB   */

        /* fat_sectors depends on total_clusters, which depends on
           fat_sectors -- a few fixed-point iterations converge. */
        fat_sectors = 1;
        for (iter = 0; iter < 8; iter++)
        {
            unsigned int data_sectors = total_sectors - reserved_sectors - num_fat_copies * fat_sectors - root_sectors;
            unsigned int clusters = data_sectors / spc;
            unsigned int bits_per_entry = (clusters < 4085) ? 12u : 16u;
            unsigned int new_fat_sectors = ((clusters + 2) * bits_per_entry / 8u + HAL_DISK_SECTOR_SIZE - 1) / HAL_DISK_SECTOR_SIZE;
            if (new_fat_sectors == fat_sectors)
                break;
            fat_sectors = new_fat_sectors;
        }

        {
            unsigned int data_sectors = total_sectors - reserved_sectors - num_fat_copies * fat_sectors - root_sectors;
            unsigned int clusters = data_sectors / spc;
            type = (clusters < 4085) ? FAT12 : FAT16;
        }
    }
    else /* FAT32: fatgen103's larger-volume cluster-size table */
    {
        reserved_sectors = 32; /* conventional: room for the boot sector, FSInfo, and their backups */
        root_entries     = 0;  /* no fixed root region on FAT32 */
        root_sectors     = 0;

             if (total_sectors <= 16777216) spc = 8;   /* <=8GB  */
        else if (total_sectors <= 33554432) spc = 16;  /* <=16GB */
        else if (total_sectors <= 67108864) spc = 32;  /* <=32GB */
        else                                  spc = 64;  /* beyond: larger clusters, still usable */

        fat_sectors = 1;
        for (iter = 0; iter < 8; iter++)
        {
            unsigned int data_sectors = total_sectors - reserved_sectors - num_fat_copies * fat_sectors;
            unsigned int clusters = data_sectors / spc;
            unsigned int new_fat_sectors = ((clusters + 2) * 4u + HAL_DISK_SECTOR_SIZE - 1) / HAL_DISK_SECTOR_SIZE;
            if (new_fat_sectors == fat_sectors)
                break;
            fat_sectors = new_fat_sectors;
        }

        type = FAT32;
    }

    for (i = 0; i < HAL_DISK_SECTOR_SIZE; i++)
        boot_sector[i] = 0;

    bpb = (struct fat_bpb*)boot_sector;
    bpb->jmp[0] = 0xEB; bpb->jmp[1] = 0x3C; bpb->jmp[2] = 0x90;
    bpb->oem[0]='O'; bpb->oem[1]='E'; bpb->oem[2]='F'; bpb->oem[3]='S';
    bpb->oem[4]=' '; bpb->oem[5]=' '; bpb->oem[6]=' '; bpb->oem[7]=' ';
    bpb->bytes_per_sector    = HAL_DISK_SECTOR_SIZE;
    bpb->sectors_per_cluster = (unsigned char)spc;
    bpb->reserved_sectors    = (unsigned short)reserved_sectors;
    bpb->num_fats            = (unsigned char)num_fat_copies;
    bpb->root_entry_count    = (unsigned short)root_entries;
    bpb->total_sectors_16    = (total_sectors < 0x10000) ? (unsigned short)total_sectors : 0;
    bpb->media_type          = 0xF8; /* fixed disk */
    bpb->fat_size_16         = (type == FAT32) ? 0 : (unsigned short)fat_sectors;
    bpb->sectors_per_track   = 63; /* unused by this driver -- LBA only, cosmetic */
    bpb->num_heads           = 16;
    bpb->hidden_sectors      = 0;
    bpb->total_sectors_32    = (total_sectors >= 0x10000) ? total_sectors : 0;

    {
        int k;

        if (type == FAT32)
        {
            struct fat32_ext_bpb* ext = (struct fat32_ext_bpb*)(boot_sector + 36);

            ext->fat_size_32        = fat_sectors;
            ext->ext_flags          = 0;
            ext->fs_version         = 0;
            ext->root_cluster       = 2;
            ext->fs_info_sector     = 1;
            ext->backup_boot_sector = 6;
            for (k = 0; k < 12; k++) ext->reserved[k] = 0;
            ext->drive_number = 0x80;
            ext->reserved1    = 0;
            ext->boot_sig     = 0x29;
            ext->volume_id    = 0x12345678u;
            for (k = 0; k < 11; k++) ext->volume_label[k] = ' ';
            ext->volume_label[0]='O'; ext->volume_label[1]='E'; ext->volume_label[2]='F'; ext->volume_label[3]='S';
            ext->fs_type[0]='F'; ext->fs_type[1]='A'; ext->fs_type[2]='T'; ext->fs_type[3]='3';
            ext->fs_type[4]='2'; ext->fs_type[5]=' '; ext->fs_type[6]=' '; ext->fs_type[7]=' ';
        }
        else
        {
            struct fat1x_ext_bpb* ext = (struct fat1x_ext_bpb*)(boot_sector + 36);

            ext->drive_number = 0x80;
            ext->reserved1    = 0;
            ext->boot_sig     = 0x29;
            ext->volume_id    = 0x12345678u;
            for (k = 0; k < 11; k++) ext->volume_label[k] = ' ';
            ext->volume_label[0]='O'; ext->volume_label[1]='E'; ext->volume_label[2]='F'; ext->volume_label[3]='S';
            ext->fs_type[0]='F'; ext->fs_type[1]='A'; ext->fs_type[2]='T';
            if (type == FAT12) { ext->fs_type[3]='1'; ext->fs_type[4]='2'; }
            else                { ext->fs_type[3]='1'; ext->fs_type[4]='6'; }
            ext->fs_type[5]=' '; ext->fs_type[6]=' '; ext->fs_type[7]=' ';
        }
    }

    boot_sector[510] = 0x55;
    boot_sector[511] = 0xAA;

    if (!disk_write(0, boot_sector))
        return 0;

    for (i = 0; i < HAL_DISK_SECTOR_SIZE; i++)
        zero_sector[i] = 0;

    /* Zero the whole reserved region beyond the primary boot sector first
       (a no-op for FAT12/16, where reserved_sectors==1) -- FAT32's backup
       boot sector and FSInfo blocks get written into this region next. */
    for (s = 1; s < reserved_sectors; s++)
        if (!disk_write(s, zero_sector))
            return 0;

    if (type == FAT32)
    {
        unsigned char fsinfo[HAL_DISK_SECTOR_SIZE];

        if (!disk_write(6, boot_sector)) /* backup boot sector, matches ext->backup_boot_sector above */
            return 0;

        /* Minimal, permissive FSInfo block: lead/struct/trail signatures
           real per spec, free_count and next_free both 0xFFFFFFFF ("unknown"
           -- a compliant driver must not trust those without a full FAT
           scan, so this is a safe default rather than a lie). */
        for (i = 0; i < HAL_DISK_SECTOR_SIZE; i++) fsinfo[i] = 0;
        fsinfo[0]=0x52; fsinfo[1]=0x52; fsinfo[2]=0x61; fsinfo[3]=0x41;
        fsinfo[484]=0x72; fsinfo[485]=0x72; fsinfo[486]=0x41; fsinfo[487]=0x61;
        fsinfo[488]=0xFF; fsinfo[489]=0xFF; fsinfo[490]=0xFF; fsinfo[491]=0xFF;
        fsinfo[492]=0xFF; fsinfo[493]=0xFF; fsinfo[494]=0xFF; fsinfo[495]=0xFF;
        fsinfo[510]=0x55; fsinfo[511]=0xAA;

        if (!disk_write(1, fsinfo))
            return 0;
        if (!disk_write(7, fsinfo)) /* backup FSInfo, alongside the backup boot sector */
            return 0;
    }

    for (i = 0; i < HAL_DISK_SECTOR_SIZE; i++)
        fat0_sector[i] = 0;

    if (type == FAT32)
    {
        fat0_sector[0]=0xF8; fat0_sector[1]=0xFF; fat0_sector[2]=0xFF; fat0_sector[3]=0x0F;  /* entry 0: media descriptor */
        fat0_sector[4]=0xFF; fat0_sector[5]=0xFF; fat0_sector[6]=0xFF; fat0_sector[7]=0x0F;  /* entry 1: EOC, reserved */
        fat0_sector[8]=0xFF; fat0_sector[9]=0xFF; fat0_sector[10]=0xFF; fat0_sector[11]=0x0F; /* entry 2: root dir cluster, EOC (single-cluster root) */
    }
    else if (type == FAT16)
    {
        fat0_sector[0] = 0xF8; fat0_sector[1] = 0xFF; /* entry 0: media descriptor + 0xFF */
        fat0_sector[2] = 0xFF; fat0_sector[3] = 0xFF; /* entry 1: EOC, reserved */
    }
    else
    {
        fat0_sector[0] = 0xF8; fat0_sector[1] = 0xFF; fat0_sector[2] = 0xFF; /* packed 12-bit entries 0/1 */
    }

    for (copy = 0; copy < num_fat_copies; copy++)
    {
        unsigned int base = reserved_sectors + copy * fat_sectors;

        if (!disk_write(base, fat0_sector))
            return 0;
        for (s = 1; s < fat_sectors; s++)
            if (!disk_write(base + s, zero_sector))
                return 0;
    }

    if (type == FAT32)
    {
        /* Root directory is cluster 2 -- one cluster, right at the start
           of the data area, zeroed (real FAT32 volumes don't put '.'/'..'
           entries at the volume root, only inside subdirectories). */
        unsigned int data_start = reserved_sectors + num_fat_copies * fat_sectors;
        for (s = 0; s < spc; s++)
            if (!disk_write(data_start + s, zero_sector))
                return 0;
    }
    else
    {
        unsigned int root_start = reserved_sectors + num_fat_copies * fat_sectors;
        for (s = 0; s < root_sectors; s++)
            if (!disk_write(root_start + s, zero_sector))
                return 0;
    }

    return 1;
}

int fat_format(void)
{
    return fat_format_at(0, hal_disk_get_sector_count());
}

static void format_short_name(const unsigned char name[8], const unsigned char ext[3], char* out)
{
    int i, n = 0;

    for (i = 0; i < 8 && name[i] != ' '; i++)
        out[n++] = (char)name[i];
    if (ext[0] != ' ')
    {
        out[n++] = '.';
        for (i = 0; i < 3 && ext[i] != ' '; i++)
            out[n++] = (char)ext[i];
    }
    out[n] = '\0';
}

/* Converts a "NAME.EXT" string into FAT's packed 11-byte 8.3 form
   (uppercase, space-padded) for direct comparison against directory
   entries. "." and ".." are FAT's two special directory names -- they are
   stored with literal dot characters in the name field and must NOT go
   through the usual name/extension split (a naive split would treat the
   dot(s) as an extension separator and produce garbage). */
static void pack_short_name(const char* name, unsigned char out[11])
{
    int i;
    int part = 0; /* 0 = name (8 bytes), 1 = ext (3 bytes) */
    int pos = 0;

    for (i = 0; i < 11; i++)
        out[i] = ' ';

    if (str_eq(name, "."))
    {
        out[0] = '.';
        return;
    }
    if (str_eq(name, ".."))
    {
        out[0] = '.';
        out[1] = '.';
        return;
    }

    for (i = 0; name[i] != '\0'; i++)
    {
        char c = name[i];

        if (c == '.')
        {
            part = 1;
            pos = 8;
            continue;
        }
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');

        if (part == 0 && pos < 8)
            out[pos++] = (unsigned char)c;
        else if (part == 1 && pos < 11)
            out[pos++] = (unsigned char)c;
    }
}

static int names_match(const struct fat_raw_dirent* e, const unsigned char packed[11])
{
    unsigned char combined[11];
    int i;

    for (i = 0; i < 8; i++) combined[i] = e->name[i];
    for (i = 0; i < 3; i++) combined[8 + i] = e->ext[i];
    for (i = 0; i < 11; i++)
        if (combined[i] != packed[i])
            return 0;
    return 1;
}

/* Iterates every real entry in `dir` (skipping deleted, volume-label, and
   long-name entries) -- a fixed LBA region for FAT12/16's root, or a
   cluster chain for FAT32's root and any subdirectory. This is the one
   place both layouts are reconciled; everything above it just works with
   raw dirents and doesn't care which kind of directory it's reading from.
   '.'/'..' entries, where present, are NOT skipped here -- callers that
   need to ignore them (e.g. an "is this directory empty" check) do so
   themselves, since some callers (fat_list_dir) want to show them. */
static int for_each_dirent_in_dir(const struct fat_dir* dir, int (*visit)(const struct fat_raw_dirent*, void*), void* ctx)
{
    unsigned char sector[HAL_DISK_SECTOR_SIZE];
    unsigned int entries_per_sector = bytes_per_sector / 32;

    if (dir->is_fixed)
    {
        unsigned int sector_index, entry_index;

        for (sector_index = 0; sector_index < dir->fixed_sectors; sector_index++)
        {
            if (!disk_read(dir->fixed_lba + sector_index, sector))
                return 0;

            for (entry_index = 0; entry_index < entries_per_sector; entry_index++)
            {
                struct fat_raw_dirent* e = (struct fat_raw_dirent*)(sector + entry_index * 32);

                if (e->name[0] == 0x00)
                    return 1;
                if (e->name[0] == 0xE5)
                    continue;
                if (e->attr == ATTR_LONG_NAME || (e->attr & ATTR_VOLUME_ID))
                    continue;
                if (!visit(e, ctx))
                    return 1;
            }
        }
        return 1;
    }
    else
    {
        unsigned int cluster = dir->start_cluster;

        while (cluster >= 2 && !is_end_of_chain(cluster))
        {
            unsigned int lba = data_start_lba + (cluster - 2) * sectors_per_cluster;
            unsigned int s, entry_index;

            for (s = 0; s < sectors_per_cluster; s++)
            {
                if (!disk_read(lba + s, sector))
                    return 0;

                for (entry_index = 0; entry_index < entries_per_sector; entry_index++)
                {
                    struct fat_raw_dirent* e = (struct fat_raw_dirent*)(sector + entry_index * 32);

                    if (e->name[0] == 0x00)
                        return 1;
                    if (e->name[0] == 0xE5)
                        continue;
                    if (e->attr == ATTR_LONG_NAME || (e->attr & ATTR_VOLUME_ID))
                        continue;
                    if (!visit(e, ctx))
                        return 1;
                }
            }

            cluster = get_fat_entry(cluster);
        }
        return 1;
    }
}

struct list_ctx
{
    struct fat_dirent* out;
    int max_entries;
    int count;
};

static int list_visit(const struct fat_raw_dirent* e, void* vctx)
{
    struct list_ctx* ctx = (struct list_ctx*)vctx;

    if (ctx->count >= ctx->max_entries)
        return 0;

    format_short_name(e->name, e->ext, ctx->out[ctx->count].name);
    ctx->out[ctx->count].size          = e->size;
    ctx->out[ctx->count].first_cluster = dirent_get_cluster(e);
    ctx->out[ctx->count].attributes    = e->attr;
    ctx->count++;
    return 1;
}

int fat_list_dir(const struct fat_dir* dir, struct fat_dirent* out, int max_entries)
{
    struct list_ctx ctx;

    if (!mounted)
        return 0;

    ctx.out = out;
    ctx.max_entries = max_entries;
    ctx.count = 0;

    for_each_dirent_in_dir(dir, list_visit, &ctx);
    return ctx.count;
}

int fat_list_root(struct fat_dirent* out, int max_entries)
{
    return fat_list_dir(&root_dir, out, max_entries);
}

struct find_ctx
{
    unsigned char packed_name[11];
    struct fat_raw_dirent found;
    int has_match;
};

static int find_visit(const struct fat_raw_dirent* e, void* vctx)
{
    struct find_ctx* ctx = (struct find_ctx*)vctx;

    if (!names_match(e, ctx->packed_name))
        return 1; /* no match, keep looking */

    ctx->found = *e;
    ctx->has_match = 1;
    return 0; /* stop */
}

static int find_dirent_in(const struct fat_dir* dir, const char* name, struct fat_raw_dirent* out)
{
    struct find_ctx ctx;

    ctx.has_match = 0;
    pack_short_name(name, ctx.packed_name);
    for_each_dirent_in_dir(dir, find_visit, &ctx);

    if (!ctx.has_match)
        return 0;

    *out = ctx.found;
    return 1;
}

int fat_find_in_dir(const struct fat_dir* dir, const char* name, struct fat_dirent* out)
{
    struct fat_raw_dirent e;

    if (!mounted)
        return 0;
    if (!find_dirent_in(dir, name, &e))
        return 0;

    format_short_name(e.name, e.ext, out->name);
    out->size          = e.size;
    out->first_cluster = dirent_get_cluster(&e);
    out->attributes    = e.attr;
    return 1;
}

void fat_get_root_dir(struct fat_dir* out)
{
    *out = root_dir;
}

int fat_dir_equal(const struct fat_dir* a, const struct fat_dir* b)
{
    if (a->is_fixed != b->is_fixed)
        return 0;
    if (a->is_fixed)
        return 1; /* only one fixed-region directory ever exists: root */
    return a->start_cluster == b->start_cluster;
}

/* Resolves a '/'-separated path to a directory. A leading '/' means
   "absolute, from root" regardless of `start`; otherwise resolution begins
   at `start`. "." is a no-op; ".." looks up the real '..' entry (written by
   fat_mkdir_in() into every subdirectory) and follows it, except at the
   volume root -- which has no '..' entry at all -- where ".." is simply
   absorbed rather than treated as an error, matching how a real shell lets
   you "cd .." past the root without complaint. */
int fat_resolve_path(const struct fat_dir* start, const char* path, struct fat_dir* out)
{
    struct fat_dir cur;
    int i = 0;

    if (!mounted)
        return 0;

    cur = (path[0] == '/') ? root_dir : *start;
    if (path[0] == '/')
        i = 1;

    while (path[i] != '\0')
    {
        char comp[FAT_MAX_NAME];
        int ci = 0;

        while (path[i] != '\0' && path[i] != '/' && ci < FAT_MAX_NAME - 1)
            comp[ci++] = path[i++];
        comp[ci] = '\0';
        if (path[i] == '/')
            i++;

        if (ci == 0 || str_eq(comp, "."))
            continue;

        if (str_eq(comp, ".."))
        {
            if (dir_is_root(&cur))
                continue; /* no '..' entry at the volume root -- stay put */

            {
                struct fat_dirent e;
                if (!fat_find_in_dir(&cur, "..", &e))
                    return 0; /* shouldn't happen -- every real subdirectory has one */

                if (e.first_cluster == 0)
                    cur = root_dir; /* FAT convention: '..' at a root's child stores cluster 0 */
                else
                {
                    cur.is_fixed = 0;
                    cur.fixed_lba = 0;
                    cur.fixed_sectors = 0;
                    cur.start_cluster = e.first_cluster;
                }
            }
            continue;
        }

        {
            struct fat_dirent e;
            if (!fat_find_in_dir(&cur, comp, &e))
                return 0;
            if (!(e.attributes & ATTR_DIRECTORY))
                return 0; /* a path component named a file, not a directory */

            cur.is_fixed = 0;
            cur.fixed_lba = 0;
            cur.fixed_sectors = 0;
            cur.start_cluster = e.first_cluster;
        }
    }

    *out = cur;
    return 1;
}

int fat_get_file_size_in(const struct fat_dir* dir, const char* name)
{
    struct fat_raw_dirent e;

    if (!mounted)
        return -1;
    if (!find_dirent_in(dir, name, &e) || (e.attr & ATTR_DIRECTORY))
        return -1;

    return (int)e.size;
}

int fat_get_file_size(const char* name)
{
    return fat_get_file_size_in(&root_dir, name);
}

int fat_read_file_in(const struct fat_dir* dir, const char* name, void* buffer, unsigned int buffer_size)
{
    struct fat_raw_dirent e;
    unsigned int cluster, bytes_read = 0;
    unsigned char* out = (unsigned char*)buffer;

    if (!mounted)
        return -1;
    if (!find_dirent_in(dir, name, &e) || (e.attr & ATTR_DIRECTORY))
        return -1;
    if (e.size > buffer_size)
        return -1;

    cluster = dirent_get_cluster(&e);

    while (!is_end_of_chain(cluster) && cluster >= 2 && bytes_read < e.size)
    {
        unsigned int lba = data_start_lba + (cluster - 2) * sectors_per_cluster;
        unsigned int s;

        for (s = 0; s < sectors_per_cluster && bytes_read < e.size; s++)
        {
            unsigned char sector[HAL_DISK_SECTOR_SIZE];
            unsigned int to_copy = e.size - bytes_read;

            if (!disk_read(lba + s, sector))
                return -1;

            if (to_copy > bytes_per_sector)
                to_copy = bytes_per_sector;

            {
                unsigned int i;
                for (i = 0; i < to_copy; i++)
                    out[bytes_read + i] = sector[i];
            }
            bytes_read += to_copy;
        }

        cluster = get_fat_entry(cluster);
    }

    return (int)bytes_read;
}

int fat_read_file(const char* name, void* buffer, unsigned int buffer_size)
{
    return fat_read_file_in(&root_dir, name, buffer, buffer_size);
}

int fat_file_exists_in(const struct fat_dir* dir, const char* name)
{
    struct fat_raw_dirent e;

    if (!mounted)
        return 0;
    return find_dirent_in(dir, name, &e);
}

int fat_file_exists(const char* name)
{
    return fat_file_exists_in(&root_dir, name);
}

unsigned long long fat_get_total_bytes(void)
{
    if (!mounted)
        return 0;
    return (unsigned long long)total_clusters * sectors_per_cluster * bytes_per_sector;
}

unsigned long long fat_get_free_bytes(void)
{
    unsigned int cluster, free_clusters = 0;

    if (!mounted)
        return 0;

    for (cluster = 2; cluster < total_clusters + 2; cluster++)
        if (get_fat_entry(cluster) == 0)
            free_clusters++;

    return (unsigned long long)free_clusters * sectors_per_cluster * bytes_per_sector;
}

/* Locates a directory's (sector, offset) slot for `name`: an existing entry
   if one matches, otherwise the first reusable slot (a deleted 0xE5 entry,
   or the 0x00 end-of-directory marker if no deleted slot was seen first).
   For a fixed-region directory (FAT12/16 root), returns 0 if the region is
   completely full with no reusable slot -- a real, if rare, limit of a
   region that can't grow. For a cluster-chain directory (FAT32 root, and
   any subdirectory), running out of room instead grows the chain by one
   more cluster and returns a slot in it. */
static int find_slot_in_dir(struct fat_dir* dir, const char* name, unsigned int* out_sector, unsigned int* out_offset, int* existed)
{
    unsigned char packed[11];
    unsigned char sector[HAL_DISK_SECTOR_SIZE];
    unsigned int entries_per_sector = bytes_per_sector / 32;
    int free_found = 0;
    unsigned int free_lba = 0, free_offset = 0;

    pack_short_name(name, packed);

    if (dir->is_fixed)
    {
        unsigned int sector_index, entry_index;

        for (sector_index = 0; sector_index < dir->fixed_sectors; sector_index++)
        {
            if (!disk_read(dir->fixed_lba + sector_index, sector))
                return 0;

            for (entry_index = 0; entry_index < entries_per_sector; entry_index++)
            {
                struct fat_raw_dirent* e = (struct fat_raw_dirent*)(sector + entry_index * 32);

                if (e->name[0] == 0x00)
                {
                    if (!free_found)
                    {
                        free_lba = dir->fixed_lba + sector_index;
                        free_offset = entry_index * 32;
                    }
                    *out_sector = free_lba;
                    *out_offset = free_offset;
                    *existed = 0;
                    return 1;
                }
                if (e->name[0] == 0xE5)
                {
                    if (!free_found)
                    {
                        free_lba = dir->fixed_lba + sector_index;
                        free_offset = entry_index * 32;
                        free_found = 1;
                    }
                    continue;
                }
                if (e->attr == ATTR_LONG_NAME || (e->attr & ATTR_VOLUME_ID))
                    continue;

                if (names_match(e, packed))
                {
                    *out_sector = dir->fixed_lba + sector_index;
                    *out_offset = entry_index * 32;
                    *existed = 1;
                    return 1;
                }
            }
        }

        if (free_found)
        {
            *out_sector = free_lba;
            *out_offset = free_offset;
            *existed = 0;
            return 1;
        }
        return 0; /* fixed region full, nothing reusable */
    }
    else
    {
        unsigned int cluster = dir->start_cluster;
        unsigned int last_cluster = cluster;

        while (cluster >= 2 && !is_end_of_chain(cluster))
        {
            unsigned int lba = data_start_lba + (cluster - 2) * sectors_per_cluster;
            unsigned int s, entry_index;

            last_cluster = cluster;

            for (s = 0; s < sectors_per_cluster; s++)
            {
                if (!disk_read(lba + s, sector))
                    return 0;

                for (entry_index = 0; entry_index < entries_per_sector; entry_index++)
                {
                    struct fat_raw_dirent* e = (struct fat_raw_dirent*)(sector + entry_index * 32);

                    if (e->name[0] == 0x00)
                    {
                        if (!free_found)
                        {
                            free_lba = lba + s;
                            free_offset = entry_index * 32;
                        }
                        *out_sector = free_lba;
                        *out_offset = free_offset;
                        *existed = 0;
                        return 1;
                    }
                    if (e->name[0] == 0xE5)
                    {
                        if (!free_found)
                        {
                            free_lba = lba + s;
                            free_offset = entry_index * 32;
                            free_found = 1;
                        }
                        continue;
                    }
                    if (e->attr == ATTR_LONG_NAME || (e->attr & ATTR_VOLUME_ID))
                        continue;

                    if (names_match(e, packed))
                    {
                        *out_sector = lba + s;
                        *out_offset = entry_index * 32;
                        *existed = 1;
                        return 1;
                    }
                }
            }

            cluster = get_fat_entry(cluster);
        }

        if (free_found)
        {
            *out_sector = free_lba;
            *out_offset = free_offset;
            *existed = 0;
            return 1;
        }

        /* Chain exhausted with no free slot -- grow it by one cluster. */
        {
            unsigned int new_cluster = alloc_cluster();
            unsigned char zero_sector[HAL_DISK_SECTOR_SIZE];
            unsigned int lba, i, s;

            if (new_cluster == 0)
                return 0; /* disk full */
            if (!set_fat_entry(last_cluster, new_cluster))
                return 0;

            for (i = 0; i < HAL_DISK_SECTOR_SIZE; i++)
                zero_sector[i] = 0;

            lba = data_start_lba + (new_cluster - 2) * sectors_per_cluster;
            for (s = 0; s < sectors_per_cluster; s++)
                if (!disk_write(lba + s, zero_sector))
                    return 0;

            *out_sector = lba;
            *out_offset = 0;
            *existed = 0;
            return 1;
        }
    }
}

static int write_dirent_at(unsigned int sector, unsigned int offset, const struct fat_raw_dirent* e)
{
    unsigned char buf[HAL_DISK_SECTOR_SIZE];

    if (!disk_read(sector, buf))
        return 0;
    *(struct fat_raw_dirent*)(buf + offset) = *e;
    return disk_write(sector, buf);
}

int fat_write_file_in(const struct fat_dir* dir, const char* name, const void* data, unsigned int size)
{
    unsigned int sector, offset;
    int existed;
    struct fat_dir mutable_dir;
    struct fat_raw_dirent e;
    const unsigned char* src = (const unsigned char*)data;
    unsigned int written = 0;
    unsigned int first_cluster = 0, prev_cluster = 0;
    unsigned char packed[11];
    int i;

    if (!mounted)
        return -1;

    mutable_dir = *dir;
    if (!find_slot_in_dir(&mutable_dir, name, &sector, &offset, &existed))
        return -1; /* directory full */

    if (existed)
    {
        unsigned char buf[HAL_DISK_SECTOR_SIZE];
        struct fat_raw_dirent* existing;

        if (!disk_read(sector, buf))
            return -1;
        existing = (struct fat_raw_dirent*)(buf + offset);
        if (existing->attr & ATTR_DIRECTORY)
            return -1; /* refuse to clobber a directory entry */
        if (dirent_get_cluster(existing) >= 2)
            free_chain(dirent_get_cluster(existing));
    }

    while (written < size)
    {
        unsigned int cluster = alloc_cluster();
        unsigned int lba, s;

        if (cluster == 0)
        {
            if (first_cluster)
                free_chain(first_cluster); /* disk filled up mid-write: don't leave a half-written file behind */
            return -1;
        }

        if (first_cluster == 0)
            first_cluster = cluster;
        else
            set_fat_entry(prev_cluster, cluster);
        prev_cluster = cluster;

        lba = data_start_lba + (cluster - 2) * sectors_per_cluster;
        for (s = 0; s < sectors_per_cluster && written < size; s++)
        {
            unsigned char buf[HAL_DISK_SECTOR_SIZE];
            unsigned int chunk = size - written;
            unsigned int b;

            if (chunk > bytes_per_sector)
                chunk = bytes_per_sector;

            for (b = 0; b < bytes_per_sector; b++)
                buf[b] = (b < chunk) ? src[written + b] : 0;

            if (!disk_write(lba + s, buf))
            {
                free_chain(first_cluster);
                return -1;
            }
            written += chunk;
        }
    }

    pack_short_name(name, packed);
    for (i = 0; i < 8; i++) e.name[i] = packed[i];
    for (i = 0; i < 3; i++) e.ext[i] = packed[8 + i];
    e.attr = 0;
    for (i = 0; i < 8; i++) e.reserved1[i] = 0;
    e.time = 0;
    e.date = 0x0021; /* 1980-01-01 -- no RTC driver yet to stamp a real date; 0 alone decodes to an invalid day/month */
    dirent_set_cluster(&e, first_cluster);
    e.size = size;

    if (!write_dirent_at(sector, offset, &e))
    {
        if (first_cluster)
            free_chain(first_cluster);
        return -1;
    }
    return (int)written;
}

int fat_write_file(const char* name, const void* data, unsigned int size)
{
    return fat_write_file_in(&root_dir, name, data, size);
}

int fat_delete_file_in(const struct fat_dir* dir, const char* name)
{
    unsigned int sector, offset;
    int existed;
    struct fat_dir mutable_dir;
    unsigned char buf[HAL_DISK_SECTOR_SIZE];
    struct fat_raw_dirent e;
    unsigned int cluster;

    if (!mounted)
        return 0;

    mutable_dir = *dir;
    if (!find_slot_in_dir(&mutable_dir, name, &sector, &offset, &existed) || !existed)
        return 0;
    if (!disk_read(sector, buf))
        return 0;

    e = *(struct fat_raw_dirent*)(buf + offset);
    if (e.attr & ATTR_DIRECTORY)
        return 0;

    cluster = dirent_get_cluster(&e);
    if (cluster >= 2)
        free_chain(cluster);

    buf[offset] = 0xE5; /* mark deleted */
    return disk_write(sector, buf);
}

int fat_delete_file(const char* name)
{
    return fat_delete_file_in(&root_dir, name);
}

/* Directory contents besides '.'/'..' means "not empty" -- real FAT
   semantics, checked by fat_rmdir_in() before it will remove anything. */
struct nonempty_ctx { int found_extra; };

static int nonempty_visit(const struct fat_raw_dirent* e, void* vctx)
{
    struct nonempty_ctx* ctx = (struct nonempty_ctx*)vctx;
    char nm[FAT_MAX_NAME];

    format_short_name(e->name, e->ext, nm);
    if (str_eq(nm, ".") || str_eq(nm, ".."))
        return 1; /* keep looking */

    ctx->found_extra = 1;
    return 0; /* stop -- already know it's non-empty */
}

int fat_mkdir_in(const struct fat_dir* parent, const char* name, struct fat_dir* out_new)
{
    struct fat_dirent existing;
    unsigned int new_cluster;
    unsigned char zero_sector[HAL_DISK_SECTOR_SIZE];
    unsigned int parent_cluster_for_dotdot;
    unsigned int lba, s, i;
    unsigned char packed[11];
    unsigned int sector, offset;
    int slot_existed;
    struct fat_dir mutable_parent;

    if (!mounted)
        return 0;
    if (fat_find_in_dir(parent, name, &existing))
        return 0; /* already exists */

    new_cluster = alloc_cluster();
    if (new_cluster == 0)
        return 0;

    for (i = 0; i < HAL_DISK_SECTOR_SIZE; i++)
        zero_sector[i] = 0;

    lba = data_start_lba + (new_cluster - 2) * sectors_per_cluster;
    for (s = 0; s < sectors_per_cluster; s++)
        if (!disk_write(lba + s, zero_sector))
        {
            free_chain(new_cluster);
            return 0;
        }

    parent_cluster_for_dotdot = dir_is_root(parent) ? 0 : parent->start_cluster;

    {
        unsigned char buf[HAL_DISK_SECTOR_SIZE];
        struct fat_raw_dirent* dot;
        struct fat_raw_dirent* dotdot;

        if (!disk_read(lba, buf))
        {
            free_chain(new_cluster);
            return 0;
        }
        dot    = (struct fat_raw_dirent*)(buf + 0);
        dotdot = (struct fat_raw_dirent*)(buf + 32);

        pack_short_name(".", packed);
        for (i = 0; i < 8; i++) dot->name[i] = packed[i];
        for (i = 0; i < 3; i++) dot->ext[i] = packed[8 + i];
        dot->attr = ATTR_DIRECTORY;
        for (i = 0; i < 8; i++) dot->reserved1[i] = 0;
        dot->time = 0;
        dot->date = 0x0021;
        dirent_set_cluster(dot, new_cluster);
        dot->size = 0;

        pack_short_name("..", packed);
        for (i = 0; i < 8; i++) dotdot->name[i] = packed[i];
        for (i = 0; i < 3; i++) dotdot->ext[i] = packed[8 + i];
        dotdot->attr = ATTR_DIRECTORY;
        for (i = 0; i < 8; i++) dotdot->reserved1[i] = 0;
        dotdot->time = 0;
        dotdot->date = 0x0021;
        dirent_set_cluster(dotdot, parent_cluster_for_dotdot);
        dotdot->size = 0;

        if (!disk_write(lba, buf))
        {
            free_chain(new_cluster);
            return 0;
        }
    }

    mutable_parent = *parent;
    if (!find_slot_in_dir(&mutable_parent, name, &sector, &offset, &slot_existed) || slot_existed)
    {
        free_chain(new_cluster);
        return 0;
    }

    {
        struct fat_raw_dirent newent;

        pack_short_name(name, packed);
        for (i = 0; i < 8; i++) newent.name[i] = packed[i];
        for (i = 0; i < 3; i++) newent.ext[i] = packed[8 + i];
        newent.attr = ATTR_DIRECTORY;
        for (i = 0; i < 8; i++) newent.reserved1[i] = 0;
        newent.time = 0;
        newent.date = 0x0021;
        dirent_set_cluster(&newent, new_cluster);
        newent.size = 0;

        if (!write_dirent_at(sector, offset, &newent))
        {
            free_chain(new_cluster);
            return 0;
        }
    }

    if (out_new)
    {
        out_new->is_fixed = 0;
        out_new->fixed_lba = 0;
        out_new->fixed_sectors = 0;
        out_new->start_cluster = new_cluster;
    }
    return 1;
}

int fat_rmdir_in(const struct fat_dir* parent, const char* name)
{
    struct fat_dirent e;
    struct fat_dir target;
    struct nonempty_ctx ctx;
    struct fat_dir mutable_parent;
    unsigned int sector, offset;
    unsigned char buf[HAL_DISK_SECTOR_SIZE];
    int existed;

    if (!mounted)
        return 0;
    if (!fat_find_in_dir(parent, name, &e))
        return 0;
    if (!(e.attributes & ATTR_DIRECTORY))
        return 0;

    target.is_fixed = 0;
    target.fixed_lba = 0;
    target.fixed_sectors = 0;
    target.start_cluster = e.first_cluster;

    ctx.found_extra = 0;
    for_each_dirent_in_dir(&target, nonempty_visit, &ctx);
    if (ctx.found_extra)
        return 0; /* not empty */

    free_chain(target.start_cluster);

    mutable_parent = *parent;
    if (!find_slot_in_dir(&mutable_parent, name, &sector, &offset, &existed) || !existed)
        return 0;
    if (!disk_read(sector, buf))
        return 0;

    buf[offset] = 0xE5; /* mark deleted */
    return disk_write(sector, buf);
}
