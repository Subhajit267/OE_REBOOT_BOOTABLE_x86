#!/usr/bin/env python3
"""Writes MBR partition table entry 1 into an existing disk image, without
touching bytes 0-445 (the bootstrap code area -- mbr.asm/kernel/src/mbr.c's
own mbr_write() both leave that alone too). Used by the Makefile's
`bootdisk.img` target to describe the FAT partition that fat_mount_at()/
mbr_find_partition() (kernel/src/mbr.c) expect to find.

Usage: write_mbr_partition.py <image> <start_lba> <sector_count>
"""
import struct
import sys

PARTITION_TABLE_OFFSET = 0x1BE
FAT32_LBA_TYPE = 0x0C


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        sys.exit(1)

    path = sys.argv[1]
    start_lba = int(sys.argv[2])
    sector_count = int(sys.argv[3])

    entry = struct.pack(
        "<B3sB3sII",
        0x80,               # boot_flag: active/bootable
        b"\x00\x00\x00",    # chs_start: unused, LBA only
        FAT32_LBA_TYPE,
        b"\x00\x00\x00",    # chs_end: unused, LBA only
        start_lba,
        sector_count,
    )
    assert len(entry) == 16

    with open(path, "r+b") as f:
        f.seek(PARTITION_TABLE_OFFSET)
        f.write(entry)
        f.write(b"\x00" * 48)  # entries 2-4: unused
        f.seek(510)
        f.write(b"\x55\xAA")

    print(f"Partition entry written: start_lba={start_lba} sector_count={sector_count}")


if __name__ == "__main__":
    main()
