/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-31
Date Last Modified: 2026-09-01
Module: HAL
File: acpi.c
About: Real ACPI S5 power-off. Locates the RSDP -> RSDT/XSDT -> FADT ->
       DSDT chain from scratch, extracts the \_S5 package's SLP_TYPa/
       SLP_TYPb via a targeted byte-pattern scan of the DSDT's AML (a
       full AML interpreter isn't needed just to read two small
       integers out of one well-known package), and writes SLP_TYP +
       SLP_EN to PM1a_CNT_BLK (and PM1b if present). Fails closed
       (returns 0, touches no hardware) at every validation step rather
       than trusting a partially-parsed or out-of-range chain.
Revisions:
- 2026-08-31  Initial creation.
- 2026-09-01  BUG FIX: every physical-address dereference in the table
              chain (XSDT/RSDT/FADT-array-entries/DSDT) now goes through
              acpi_addr_safe(), bounds-checked against paging.h's real
              identity-map limit, before being touched. Previously
              assumed (per this file's own old comment) that ACPI
              tables always live in low memory below the paging cap --
              false on real firmware: a 512MB-RAM VM page-faulted here
              at ~0x1FF00000 because SeaBIOS had placed the real tables
              near the top of RAM. Unreachable in QEMU specifically
              (its own debug-shutdown ports, tried first in
              hal_power_off(), always intercept before this runs) --
              reproduces on hypervisors that don't implement those
              ports (VMware/VirtualBox). Verified by temporarily
              disabling those debug ports and booting with -m 512: a
              clean "Shutting down." + safe hlt loop instead of a page
              fault.
------------------------------------------------------------
*/

#include "acpi.h"
#include "io_ports.h"
#include "paging.h"

/* Paging is identity-mapped for physical memory up to paging_get_identity_
   limit() (see kernel/src/paging.c) -- find_rsdp()'s own scan ranges (EBDA,
   0xE0000-0xFFFFF) are always comfortably below that, but the RSDT/XSDT/
   FADT/DSDT chain this file walks afterward is firmware-placed and can
   legitimately sit anywhere in RAM (SeaBIOS in particular puts these tables
   near the top of RAM, not in low memory) -- confirmed the hard way: a
   512MB-RAM VM page-faulted inside this file at ~0x1FF00000, just under the
   256MB identity-map cap's old blind-trust assumption. acpi_addr_safe()
   below gates every such pointer before it's dereferenced, so a table that
   happens to live past the mapped region is treated as "ACPI unavailable"
   (fail closed, same as every other failure path here) instead of crashing
   the whole shutdown/reboot path. */
static int acpi_addr_safe(unsigned int addr, unsigned int size)
{
    unsigned long long limit = paging_get_identity_limit();
    return addr != 0 && (unsigned long long)addr + size <= limit;
}

struct acpi_sdt_header
{
    char           sig[4];
    unsigned int   length;
    unsigned char  revision;
    unsigned char  checksum;
    char           oem_id[6];
    char           oem_table_id[8];
    unsigned int   oem_revision;
    unsigned int   creator_id;
    unsigned int   creator_revision;
} __attribute__((packed));

struct rsdp_v1
{
    char           sig[8]; /* "RSD PTR " */
    unsigned char  checksum;
    char           oem_id[6];
    unsigned char  revision;
    unsigned int   rsdt_address;
} __attribute__((packed));

struct rsdp_v2
{
    struct rsdp_v1 v1;
    unsigned int   length;
    unsigned long long xsdt_address;
    unsigned char  extended_checksum;
    unsigned char  reserved[3];
} __attribute__((packed));

/* Standard published FADT layout (fields through the ACPI 2.0+ 64-bit
   extension block) -- offsets come from the struct layout itself, not
   hand-computed, since a packed struct matching the real spec lays them
   out identically to how the firmware wrote them. */
struct fadt
{
    struct acpi_sdt_header h;
    unsigned int   firmware_ctrl;
    unsigned int   dsdt;
    unsigned char  reserved0;
    unsigned char  preferred_pm_profile;
    unsigned short sci_interrupt;
    unsigned int   smi_command_port;
    unsigned char  acpi_enable;
    unsigned char  acpi_disable;
    unsigned char  s4bios_req;
    unsigned char  pstate_control;
    unsigned int   pm1a_event_block;
    unsigned int   pm1b_event_block;
    unsigned int   pm1a_control_block;
    unsigned int   pm1b_control_block;
    unsigned int   pm2_control_block;
    unsigned int   pm_timer_block;
    unsigned int   gpe0_block;
    unsigned int   gpe1_block;
    unsigned char  pm1_event_length;
    unsigned char  pm1_control_length;
    unsigned char  pm2_control_length;
    unsigned char  pm2_control_offset;
    unsigned char  pm_timer_length;
    unsigned char  gpe0_length;
    unsigned char  gpe1_length;
    unsigned char  gpe1_base;
    unsigned char  cstate_control;
    unsigned short worst_c2_latency;
    unsigned short worst_c3_latency;
    unsigned short flush_size;
    unsigned short flush_stride;
    unsigned char  duty_offset;
    unsigned char  duty_width;
    unsigned char  day_alarm;
    unsigned char  month_alarm;
    unsigned char  century;
    unsigned short boot_arch_flags;
    unsigned char  reserved1;
    unsigned int   flags;
    unsigned char  reset_reg[12];
    unsigned char  reset_value;
    unsigned char  reserved2[3];
    unsigned long long x_firmware_ctrl;
    unsigned long long x_dsdt;
    unsigned char  x_pm1a_event_block[12];
    unsigned char  x_pm1b_event_block[12];
    unsigned char  x_pm1a_control_block[12];
    unsigned char  x_pm1b_control_block[12];
    unsigned char  x_pm2_control_block[12];
    unsigned char  x_pm_timer_block[12];
    unsigned char  x_gpe0_block[12];
    unsigned char  x_gpe1_block[12];
} __attribute__((packed));

static int bytes_equal(const void* a, const void* b, unsigned int n)
{
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    unsigned int i;
    for (i = 0; i < n; i++)
        if (pa[i] != pb[i])
            return 0;
    return 1;
}

static int checksum_ok(const void* data, unsigned int len)
{
    const unsigned char* p = (const unsigned char*)data;
    unsigned char sum = 0;
    unsigned int i;
    for (i = 0; i < len; i++)
        sum = (unsigned char)(sum + p[i]);
    return sum == 0;
}

static const struct rsdp_v1* find_rsdp(void)
{
    unsigned short ebda_seg = *(volatile unsigned short*)0x40E;
    unsigned int addr;

    if (ebda_seg != 0)
    {
        unsigned int ebda_addr = (unsigned int)ebda_seg << 4;
        for (addr = ebda_addr; addr < ebda_addr + 1024; addr += 16)
        {
            if (bytes_equal((const void*)addr, "RSD PTR ", 8))
                return (const struct rsdp_v1*)addr;
        }
    }

    for (addr = 0xE0000; addr < 0x100000; addr += 16)
    {
        if (bytes_equal((const void*)addr, "RSD PTR ", 8))
            return (const struct rsdp_v1*)addr;
    }

    return 0;
}

static const struct fadt* find_fadt_in_table_array(const unsigned char* array_base,
                                                     unsigned int entry_count,
                                                     int entries_are_64bit)
{
    unsigned int i;

    for (i = 0; i < entry_count; i++)
    {
        unsigned int table_addr;

        if (entries_are_64bit)
        {
            unsigned long long p;
            const unsigned char* entry = array_base + i * 8;
            unsigned int j;
            p = 0;
            for (j = 0; j < 8; j++)
                p |= ((unsigned long long)entry[j]) << (8 * j);
            table_addr = (unsigned int)p; /* every real ACPI table this project will see lives well below 4GB */
        }
        else
        {
            const unsigned char* entry = array_base + i * 4;
            table_addr = (unsigned int)entry[0] | ((unsigned int)entry[1] << 8) |
                         ((unsigned int)entry[2] << 16) | ((unsigned int)entry[3] << 24);
        }

        if (!acpi_addr_safe(table_addr, sizeof(struct acpi_sdt_header)))
            continue;

        {
            const struct acpi_sdt_header* h = (const struct acpi_sdt_header*)table_addr;
            if (bytes_equal(h->sig, "FACP", 4) && acpi_addr_safe(table_addr, h->length)
                && checksum_ok(h, h->length))
                return (const struct fadt*)table_addr;
        }
    }
    return 0;
}

/* Scans raw AML for the \_S5 package and pulls out SLP_TYPa/SLP_TYPb the
   standard non-full-AML-interpreter way: find the "_S5_" name, confirm it's
   immediately followed by a PackageOp (0x12), skip the package-length
   encoding, then read the two byte-sized sleep-type constants. */
static int parse_s5(const struct acpi_sdt_header* dsdt, unsigned int* slp_typ_a, unsigned int* slp_typ_b)
{
    const unsigned char* base = (const unsigned char*)dsdt + sizeof(struct acpi_sdt_header);
    unsigned int len = dsdt->length - (unsigned int)sizeof(struct acpi_sdt_header);
    unsigned int i;
    const unsigned char* s5 = 0;

    if (dsdt->length < sizeof(struct acpi_sdt_header) + 4)
        return 0;

    for (i = 0; i + 4 <= len; i++)
    {
        if (bytes_equal(base + i, "_S5_", 4))
        {
            s5 = base + i;
            break;
        }
    }
    if (!s5)
        return 0;

    /* Expect PackageOp (0x12) right after the name, per the ACPI spec's
       DefinitionBlock encoding of a Name(_S5, Package(){...}) statement. */
    if (s5[4] != 0x12)
        return 0;

    {
        const unsigned char* p = s5 + 5;
        unsigned int pkg_len_bytes = ((p[0] & 0xC0) >> 6); /* top 2 bits of PkgLength lead byte: how many extra length bytes follow */
        p += 1 + pkg_len_bytes; /* skip PkgLength encoding */
        p += 1;                 /* skip NumElements byte */

        if (*p == 0x0A) p++;    /* BytePrefix -- constant is a single byte, skip the prefix */
        *slp_typ_a = *p;
        p++;

        if (*p == 0x0A) p++;
        *slp_typ_b = *p;
    }
    return 1;
}

int acpi_power_off(void)
{
    const struct rsdp_v1* rsdp1;
    const struct fadt* fadt = 0;
    unsigned int slp_typ_a = 0, slp_typ_b = 0;
    const struct acpi_sdt_header* dsdt;
    unsigned int pm1a_port, pm1b_port;

    rsdp1 = find_rsdp();
    if (!rsdp1)
        return 0;
    if (!checksum_ok(rsdp1, sizeof(struct rsdp_v1)))
        return 0;

    if (rsdp1->revision >= 2)
    {
        const struct rsdp_v2* rsdp2 = (const struct rsdp_v2*)rsdp1;
        if (!checksum_ok(rsdp2, sizeof(struct rsdp_v2)))
            return 0; /* extended part failed to validate -- fall back to the v1 RSDT below instead of trusting a corrupt XSDT pointer */

        if (rsdp2->xsdt_address != 0 && acpi_addr_safe((unsigned int)rsdp2->xsdt_address, sizeof(struct acpi_sdt_header)))
        {
            const struct acpi_sdt_header* xsdt = (const struct acpi_sdt_header*)(unsigned int)rsdp2->xsdt_address;
            if (acpi_addr_safe((unsigned int)rsdp2->xsdt_address, xsdt->length) && checksum_ok(xsdt, xsdt->length))
            {
                unsigned int count = (xsdt->length - (unsigned int)sizeof(struct acpi_sdt_header)) / 8;
                fadt = find_fadt_in_table_array((const unsigned char*)xsdt + sizeof(struct acpi_sdt_header), count, 1);
            }
        }
    }

    if (!fadt)
    {
        const struct acpi_sdt_header* rsdt;

        if (!acpi_addr_safe(rsdp1->rsdt_address, sizeof(struct acpi_sdt_header)))
            return 0;
        rsdt = (const struct acpi_sdt_header*)rsdp1->rsdt_address;
        if (!acpi_addr_safe(rsdp1->rsdt_address, rsdt->length) || !checksum_ok(rsdt, rsdt->length))
            return 0;
        {
            unsigned int count = (rsdt->length - (unsigned int)sizeof(struct acpi_sdt_header)) / 4;
            fadt = find_fadt_in_table_array((const unsigned char*)rsdt + sizeof(struct acpi_sdt_header), count, 0);
        }
    }

    if (!fadt)
        return 0;

    dsdt = 0;
    if (fadt->h.length >= 148 && fadt->x_dsdt != 0 && acpi_addr_safe((unsigned int)fadt->x_dsdt, sizeof(struct acpi_sdt_header)))
        dsdt = (const struct acpi_sdt_header*)(unsigned int)fadt->x_dsdt;
    else if (fadt->dsdt != 0 && acpi_addr_safe(fadt->dsdt, sizeof(struct acpi_sdt_header)))
        dsdt = (const struct acpi_sdt_header*)fadt->dsdt;

    if (!dsdt || !bytes_equal(dsdt->sig, "DSDT", 4))
        return 0;
    if (!acpi_addr_safe((unsigned int)dsdt, dsdt->length) || !checksum_ok(dsdt, dsdt->length))
        return 0;

    if (!parse_s5(dsdt, &slp_typ_a, &slp_typ_b))
        return 0;

    pm1a_port = fadt->pm1a_control_block;
    pm1b_port = fadt->pm1b_control_block;

    if (pm1a_port == 0)
        return 0;

    outw((unsigned short)pm1a_port, (unsigned short)((slp_typ_a << 10) | (1 << 13)));
    if (pm1b_port != 0)
        outw((unsigned short)pm1b_port, (unsigned short)((slp_typ_b << 10) | (1 << 13)));

    return 1;
}
