/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-03
Date Last Modified: 2026-03-03
Module: PAL
File: pal_oe_info.h
About: Advanced OE Information Abstraction Layer.
       Provides detailed CPU and Memory information
       independent of backend.

Revisions:
- 2026-03-03  Initial creation
- 2026-03-03  Extended CPU + RAM fields
------------------------------------------------------------
*/

#ifndef PAL_OE_INFO_H
#define PAL_OE_INFO_H

typedef struct
{
    /* CPU Info */
    char cpu_name[128];
    int cpu_cores;
    int cpu_threads;
    unsigned long cpu_mhz;
    char architecture[16];

    /* Memory Info */
    unsigned long long total_ram;
    unsigned long long free_ram;

    /* Backend */
    const char *backend_name;

    /* Disk Info */
    unsigned long long disk_total;
    unsigned long long disk_free;

    /* Uptime (seconds) */
    unsigned long long uptime_seconds;

} pal_oe_info_t;

int pal_get_oe_info(pal_oe_info_t *info);

#endif