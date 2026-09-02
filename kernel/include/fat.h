/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-31
Module: Kernel
File: fat.h
About: FAT12/16/32 filesystem driver (this project's on-disk format,
       branded "OEFS" but byte-for-byte standard FAT -- readable by
       mtools/any other OS). Path-aware, with real subdirectory support
       via struct fat_dir (a fixed LBA region for FAT12/16's root, or a
       cluster chain for FAT32's root and every subdirectory) and
       fat_resolve_path(). Root-only convenience wrappers are kept for
       existing callers that never dealt with subdirectories.
Revisions:
- 2026-08-30  Initial creation (Phase 5): FAT12/16 mount/read, root-
              directory-only, no write support yet.
- 2026-08-30  Write support added (Phase 7): fat_write_file/delete_file,
              full FAT-copy propagation.
- 2026-08-31  Rewritten for FAT32 + real subdirectories: struct fat_dir
              generalized (fixed region vs. cluster chain), fat_dirent's
              first_cluster widened to 32-bit (FAT32 cluster numbers
              exceed 65535), path-aware fat_resolve_path()/`_in()` API
              added, root-only functions kept as thin wrappers.
------------------------------------------------------------
*/

#ifndef KERNEL_FAT_H
#define KERNEL_FAT_H

#define FAT_MAX_NAME 13 /* 8.3 name + '.' + NUL */

#define FAT_ATTR_DIRECTORY 0x10

struct fat_dirent
{
    char name[FAT_MAX_NAME];
    unsigned int size;
    unsigned int first_cluster; /* full 32-bit cluster number (needed for real FAT32 volumes) */
    unsigned char attributes;
};

/* Describes a directory's on-disk layout: either a fixed LBA region
   (FAT12/16's root, which can't grow) or a cluster chain (FAT32's root, and
   any subdirectory on any FAT type). Every fat_*_in()/fat_resolve_path()
   call takes one of these instead of assuming root, which is what makes
   subdirectories possible. Get the root one via fat_get_root_dir(); get any
   other one by resolving a path or by fat_mkdir_in()'s out_new parameter. */
struct fat_dir
{
    int is_fixed;
    unsigned int fixed_lba;
    unsigned int fixed_sectors;
    unsigned int start_cluster;
};

/* Reads the boot sector via hal_disk_read_sector() and parses the BPB.
   Supports FAT12, FAT16, and FAT32 ("OEFS"-branded, but a real, standard
   FAT32 volume byte-for-byte -- mtools/any other OS can still read it).
   FAT12/16 volumes are superfloppy-style (no partition table, fixed-region
   root directory); FAT32's root directory is a normal cluster chain like
   any subdirectory. Returns 1 on a recognized volume, 0 otherwise. Call
   after hal_disk_init() succeeds. */
int fat_mount(void);

/* Same as fat_mount(), but the volume starts at `partition_start_lba`
   instead of LBA 0 -- every LBA this driver computes internally is relative
   to that base. `fat_mount()` is a thin wrapper for `fat_mount_at(0)`
   (today's unpartitioned "superfloppy" behavior, unchanged). Use this after
   mbr_find_partition() (kernel/include/mbr.h) locates a real partition. */
int fat_mount_at(unsigned int partition_start_lba);

/* Formats the disk from scratch: writes a fresh boot sector/BPB, both FAT
   copies, and a zeroed root directory -- the "mkfs" a real installer runs
   against a genuinely blank disk. Picks FAT12/16 (fixed-size root, cluster
   size up to 2GB volumes) or FAT32 (cluster-chain root, for larger volumes)
   automatically by disk size, following Microsoft's fatgen103 cluster-size
   tables for each. Call fat_mount() again afterward -- this doesn't mount,
   just writes. Returns 1 on success, 0 if the disk is unusable. */
int fat_format(void);

/* Same as fat_format(), but formats `partition_sector_count` sectors
   starting at `partition_start_lba` instead of the whole disk from LBA 0.
   `fat_format()` is a thin wrapper for
   `fat_format_at(0, hal_disk_get_sector_count())` (today's behavior,
   unchanged). Use this to format a specific partition rather than the
   entire disk. */
int fat_format_at(unsigned int partition_start_lba, unsigned int partition_sector_count);

/* Fills `out` with a descriptor for the mounted volume's root directory --
   the starting point for fat_resolve_path() and every fat_*_in() call. */
void fat_get_root_dir(struct fat_dir* out);

/* Resolves a '/'-separated path to a directory, starting from `start`
   (ignored if `path` begins with '/', which is always root-relative). "."
   is a no-op component; ".." looks up the real '..' entry every
   subdirectory has (fat_mkdir_in() always writes one) -- at the volume
   root, where no '..' entry exists, ".." is absorbed as a no-op rather than
   an error. Every path component but the last must name a directory,
   including the last if the whole path names a directory rather than a
   file. Returns 1 with `out` filled in on success, 0 if any component is
   missing or names a file where a directory was expected. */
int fat_resolve_path(const struct fat_dir* start, const char* path, struct fat_dir* out);

/* True if `a` and `b` describe the same directory. */
int fat_dir_equal(const struct fat_dir* a, const struct fat_dir* b);

/* Looks up a single 8.3 name directly inside `dir` (no path walking).
   Returns 1 with `out` filled in on a match, 0 otherwise. */
int fat_find_in_dir(const struct fat_dir* dir, const char* name, struct fat_dirent* out);

/* Lists `dir`'s entries into `out` (capped at max_entries), skipping
   deleted, volume-label, and long-name entries -- '.'/'..' entries in a
   subdirectory ARE included, same as any real FAT driver. Returns the
   count written. */
int fat_list_dir(const struct fat_dir* dir, struct fat_dirent* out, int max_entries);

/* Creates a new subdirectory named `name` directly inside `parent` (no path
   walking -- resolve the parent first via fat_resolve_path() for a nested
   path). Allocates a cluster, zeroes it, and writes real '.' (self) and
   '..' (parent -- cluster 0 if `parent` is the volume root, matching real
   FAT convention) entries into it before linking the new directory into
   `parent`. If `out_new` is non-NULL it's filled in with a descriptor for
   the new directory. Returns 1 on success, 0 if `name` already exists in
   `parent` or the disk is full. */
int fat_mkdir_in(const struct fat_dir* parent, const char* name, struct fat_dir* out_new);

/* Removes the subdirectory named `name` from `parent`. Refuses (returns 0)
   unless the directory contains nothing but '.'/'..' -- real FAT semantics,
   matching every other FAT driver. Frees its cluster chain and marks its
   entry in `parent` reusable on success. */
int fat_rmdir_in(const struct fat_dir* parent, const char* name);

/* Path/directory-scoped equivalents of the root-only functions below --
   `dir` is any directory returned by fat_get_root_dir()/fat_resolve_path()/
   fat_mkdir_in(), `name` is a single 8.3 name directly inside it (no further
   path walking -- resolve the containing directory first). */
int fat_read_file_in(const struct fat_dir* dir, const char* name, void* buffer, unsigned int buffer_size);
int fat_get_file_size_in(const struct fat_dir* dir, const char* name);
int fat_file_exists_in(const struct fat_dir* dir, const char* name);
int fat_write_file_in(const struct fat_dir* dir, const char* name, const void* data, unsigned int size);
int fat_delete_file_in(const struct fat_dir* dir, const char* name);

/* Root-directory-only convenience wrappers, kept for existing callers that
   never dealt with subdirectories (e.g. the flat user.bd/pwd.bd/ .RG files
   the hosted app writes at its own root). Equivalent to calling the `_in`
   functions above with fat_get_root_dir()'s result. */
int fat_list_root(struct fat_dirent* out, int max_entries);
int fat_read_file(const char* name, void* buffer, unsigned int buffer_size);
int fat_get_file_size(const char* name);
int fat_file_exists(const char* name);
int fat_write_file(const char* name, const void* data, unsigned int size);
int fat_delete_file(const char* name);

unsigned long long fat_get_total_bytes(void);
unsigned long long fat_get_free_bytes(void);

#endif
