/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-03
Date Last Modified: 2026-04-13
Module: System Tools
File: systeminfo.c
About: OE System Information Tool – fully macro‑based coordinates.
       All positions defined in ui_coordinates.h.
       Page 1: OE Build + PAL + User
       Page 2: CPU Information + System Uptime
       Page 3: Memory + Disk Information
Revisions:
       - 2026-03-03  Initial advanced version
       - 2026-03-03  Unified column layout
       - 2026-03-03  Multi-color semantic design
       - 2026-04-13  NEW 80*25 Layout implemented
       - 2026-08-26  BUG FIX: the Build Date row printed OE_BUILD_DATE
                     (10 chars) at UI_SYSINFO_VAL_COL and OE_BUILD_TIME
                     at UI_SYSINFO_UNIT_COL, only 8 columns apart —
                     the date's tail was always overwritten by the
                     time. Now printed as one combined value string.
                     Also: info->cpu_name (up to 127 chars) and the
                     current username (up to 63 chars) were printed
                     with no cap and could run past the box's right
                     border/column 79 on real hardware or a long
                     username — both now display via a bounded copy.
------------------------------------------------------------
*/

#include "systeminfo.h"
#include "pal.h"
#include "pal_oe_info.h"
#include "ui_setup.h"
#include "ui_elements.h"
#include "user.h"
#include "branding.h"

/* Box strings sized for 58 chars */
#define SI_TOP   " ======================================================== "
#define SI_SEP   "| ------------------------------------------------------ |"
#define SI_BLANK "|                                                        |"
#define SI_BOT   " ======================================================== "

static void format_uptime(unsigned long long seconds, int* h, int* m, int* s)
{
    *h = (int)(seconds / 3600);
    *m = (int)((seconds % 3600) / 60);
    *s = (int)(seconds % 60);
}

/* Draw a labelled row at the given row (macro expected) */
static void si_row(int row, const char* label, const char* value, const char* unit)
{
    ui_title(row, UI_SYSINFO_BOX_COL, BLUE bold, "| ");
    ui_title(row, UI_SYSINFO_LBL_COL, yellow bold, label);
    ui_title(row, UI_SYSINFO_VAL_COL, GREEN, value);
    if (unit && unit[0])
        ui_title(row, UI_SYSINFO_UNIT_COL, white, unit);
    ui_title(row, UI_SYSINFO_RB, BLUE bold, "|");
}

static void si_row_int(int row, const char* label, int value, const char* unit)
{
    ui_title(row, UI_SYSINFO_BOX_COL, BLUE bold, "| ");
    ui_title(row, UI_SYSINFO_LBL_COL, yellow bold, label);
    ui_title_int(row, UI_SYSINFO_VAL_COL, GREEN, value);
    if (unit && unit[0])
        ui_title(row, UI_SYSINFO_UNIT_COL, white, unit);
    ui_title(row, UI_SYSINFO_RB, BLUE bold, "|");
}

/* Draw a section header (uses row, row+1, row+2) */
static void si_section(int row, const char* title)
{
    ui_title(row, UI_SYSINFO_BOX_COL, BLUE bold, SI_BLANK);
    ui_title(row + 1, UI_SYSINFO_BOX_COL, BLUE bold, "| ");
    ui_title(row + 1, UI_SYSINFO_LBL_COL, cyan, title);
    ui_title(row + 1, UI_SYSINFO_RB, BLUE bold, "|");
    ui_title(row + 2, UI_SYSINFO_BOX_COL, BLUE bold, SI_SEP);
}

/* ================================================================
   PAGE 1
   ================================================================ */
static void sysinfo_page1(const pal_oe_info_t* info)
{
    /* Top border + title */
    ui_title(UI_SYSINFO_TOP_ROW, UI_SYSINFO_BOX_COL, BLUE bold, SI_TOP);
    ui_title(UI_SYSINFO_TITLE_ROW, UI_SYSINFO_BOX_COL, BLUE bold, "|");
    ui_title(UI_SYSINFO_TITLE_ROW, UI_SYSINFO_BOX_COL + 10, RED bold underline,
             OE_SHORT_NAME " SYSTEM INFORMATION");
    ui_title(UI_SYSINFO_TITLE_ROW, UI_SYSINFO_RB, BLUE bold, "|");
    ui_title(UI_SYSINFO_TITLE2_ROW, UI_SYSINFO_BOX_COL, BLUE bold, SI_TOP);

    /* OE Build */
    si_section(UI_SYSINFO_P1_SECT1_ROW, "OE BUILD DETAILS:");
    si_row(UI_SYSINFO_P1_VER_ROW,   "Version:",     OE_VERSION, "");
    si_row(UI_SYSINFO_P1_TYPE_ROW,  "Build Type:",  OE_BUILD_TYPE, "");
    si_row(UI_SYSINFO_P1_DATE_ROW,  "Build Date:",  OE_BUILD_DATE "  " OE_BUILD_TIME, "");
    si_row(UI_SYSINFO_P1_DEV_ROW,   "Developer:",   OE_DEVELOPER, "");

    /* User */
    ui_title(UI_SYSINFO_P1_USER_ROW, UI_SYSINFO_BOX_COL, BLUE bold, "| ");
    ui_title(UI_SYSINFO_P1_USER_ROW, UI_SYSINFO_LBL_COL, yellow bold, "User:");
    {
        /* fits within the box: VAL_COL=35 to right border RB=76 */
        char user_disp[42];
        pal_strncpy(user_disp, user_exists() ? current_user : "GUEST", sizeof(user_disp));
        ui_title(UI_SYSINFO_P1_USER_ROW, UI_SYSINFO_VAL_COL, red bold, user_disp);
    }
    ui_title(UI_SYSINFO_P1_USER_ROW, UI_SYSINFO_RB, BLUE bold, "|");

    /* PAL Runtime */
    si_section(UI_SYSINFO_P1_SECT2_ROW, "PAL RUNTIME:");
    si_row(UI_SYSINFO_P1_BACKEND_ROW, "Backend:", info->backend_name, "");

    /* Filler lines */
    ui_title(UI_SYSINFO_Filler1_ROW, UI_SYSINFO_BOX_COL, BLUE bold, "|");
    ui_title(UI_SYSINFO_Filler1_ROW, UI_SYSINFO_RB, BLUE bold, "|");
    ui_title(UI_SYSINFO_Filler2_ROW, UI_SYSINFO_BOX_COL, BLUE bold, "|");
    ui_title(UI_SYSINFO_Filler2_ROW, UI_SYSINFO_RB, BLUE bold, "|");

    /* Bottom border */
    ui_title(UI_SYSINFO_BOT_ROW, UI_SYSINFO_BOX_COL, BLUE bold, SI_BOT);
}

/* ================================================================
   PAGE 2
   ================================================================ */
static void sysinfo_page2(const pal_oe_info_t* info)
{
    int h, m, s;
    format_uptime(info->uptime_seconds, &h, &m, &s);

    ui_title(UI_SYSINFO_TOP_ROW, UI_SYSINFO_BOX_COL, BLUE bold, SI_TOP);
    ui_title(UI_SYSINFO_TITLE_ROW, UI_SYSINFO_BOX_COL, BLUE bold, "|");
    ui_title(UI_SYSINFO_TITLE_ROW, UI_SYSINFO_BOX_COL + 10, RED bold underline,
             OE_SHORT_NAME " SYSTEM INFORMATION");
    ui_title(UI_SYSINFO_TITLE_ROW, UI_SYSINFO_RB, BLUE bold, "|");
    ui_title(UI_SYSINFO_TITLE2_ROW, UI_SYSINFO_BOX_COL, BLUE bold, SI_TOP);

    /* CPU */
    si_section(UI_SYSINFO_P2_SECT1_ROW, "CPU INFORMATION:");
    {
        /* fits within the box: VAL_COL=35 to right border RB=76 */
        char cpu_disp[42];
        pal_strncpy(cpu_disp, info->cpu_name, sizeof(cpu_disp));
        si_row(UI_SYSINFO_P2_MODEL_ROW, "Model:", cpu_disp, "");
    }
    si_row_int(UI_SYSINFO_P2_CORES_ROW,   "Cores:",    info->cpu_cores, "");
    si_row_int(UI_SYSINFO_P2_THREADS_ROW, "Threads:",  info->cpu_threads, "");
    si_row_int(UI_SYSINFO_P2_MHZ_ROW,     "Clock Speed:", (int)info->cpu_mhz, "MHz");
    si_row(UI_SYSINFO_P2_ARCH_ROW,    "Architecture:", info->architecture, "");

    /* Uptime */
    si_section(UI_SYSINFO_P2_SECT2_ROW, "SYSTEM UPTIME:");
    ui_title(UI_SYSINFO_P2_UPTIME_ROW, UI_SYSINFO_BOX_COL, BLUE bold, "| ");
    ui_title(UI_SYSINFO_P2_UPTIME_ROW, UI_SYSINFO_LBL_COL, yellow bold, "Uptime:");
    ui_title_int(UI_SYSINFO_P2_UPTIME_ROW, UI_SYSINFO_VAL_COL, GREEN, h);
    ui_title(UI_SYSINFO_P2_UPTIME_ROW, UI_SYSINFO_VAL_COL + 4, white, "h");
    ui_title_int(UI_SYSINFO_P2_UPTIME_ROW, UI_SYSINFO_VAL_COL + 7, GREEN, m);
    ui_title(UI_SYSINFO_P2_UPTIME_ROW, UI_SYSINFO_VAL_COL + 11, white, "m");
    ui_title_int(UI_SYSINFO_P2_UPTIME_ROW, UI_SYSINFO_VAL_COL + 14, GREEN, s);
    ui_title(UI_SYSINFO_P2_UPTIME_ROW, UI_SYSINFO_VAL_COL + 18, white, "s");
    ui_title(UI_SYSINFO_P2_UPTIME_ROW, UI_SYSINFO_RB, BLUE bold, "|");

    /* Filler lines */
    ui_title(UI_SYSINFO_Filler1_ROW, UI_SYSINFO_BOX_COL, BLUE bold, "|");
    ui_title(UI_SYSINFO_Filler1_ROW, UI_SYSINFO_RB, BLUE bold, "|");
    ui_title(UI_SYSINFO_Filler2_ROW, UI_SYSINFO_BOX_COL, BLUE bold, "|");
    ui_title(UI_SYSINFO_Filler2_ROW, UI_SYSINFO_RB, BLUE bold, "|");

    ui_title(UI_SYSINFO_BOT_ROW, UI_SYSINFO_BOX_COL, BLUE bold, SI_BOT);
}

/* ================================================================
   PAGE 3
   ================================================================ */
static void sysinfo_page3(const pal_oe_info_t* info)
{
    unsigned long long total = info->total_ram / (1024ULL * 1024ULL);
    unsigned long long fr = info->free_ram / (1024ULL * 1024ULL);
    unsigned long long used = total - fr;
    int percent = (total != 0) ? (int)((used * 100) / total) : 0;

    unsigned long long dtotal = info->disk_total / (1024ULL * 1024ULL * 1024ULL);
    unsigned long long dfree = info->disk_free / (1024ULL * 1024ULL * 1024ULL);
    unsigned long long dused = (info->disk_total - info->disk_free)
        / (1024ULL * 1024ULL * 1024ULL);
    int dpct = (dtotal != 0) ? (int)((dused * 100) / dtotal) : 0;

    ui_title(UI_SYSINFO_TOP_ROW, UI_SYSINFO_BOX_COL, BLUE bold, SI_TOP);
    ui_title(UI_SYSINFO_TITLE_ROW, UI_SYSINFO_BOX_COL, BLUE bold, "|");
    ui_title(UI_SYSINFO_TITLE_ROW, UI_SYSINFO_BOX_COL + 10, RED bold underline,
             OE_SHORT_NAME " SYSTEM INFORMATION");
    ui_title(UI_SYSINFO_TITLE_ROW, UI_SYSINFO_RB, BLUE bold, "|");
    ui_title(UI_SYSINFO_TITLE2_ROW, UI_SYSINFO_BOX_COL, BLUE bold, SI_TOP);

    /* Memory */
    si_section(UI_SYSINFO_P3_SECT1_ROW, "MEMORY INFORMATION:");
    si_row_int(UI_SYSINFO_P3_RAM_TOT_ROW,  "Total RAM:", (int)total, "MB");
    si_row_int(UI_SYSINFO_P3_RAM_FREE_ROW, "Free RAM:",  (int)fr,    "MB");
    si_row_int(UI_SYSINFO_P3_RAM_USED_ROW, "Used RAM:",  (int)used,  "MB");
    si_row_int(UI_SYSINFO_P3_RAM_PCT_ROW,  "Usage:",     percent,    "%");

    /* Disk */
    si_section(UI_SYSINFO_P3_SECT2_ROW, "DISK INFORMATION:");
    si_row_int(UI_SYSINFO_P3_DISK_TOT_ROW,  "Total Disk:", (int)dtotal, "GB");
    si_row_int(UI_SYSINFO_P3_DISK_FREE_ROW, "Free Disk:",  (int)dfree,  "GB");
    si_row_int(UI_SYSINFO_P3_DISK_USED_ROW, "Used Disk:",  (int)dused,  "GB");
    si_row_int(UI_SYSINFO_P3_DISK_PCT_ROW,  "Usage:",      dpct,        "%");

    ui_title(UI_SYSINFO_BOT_ROW, UI_SYSINFO_BOX_COL, BLUE bold, SI_BOT);
}

/* ================================================================
   ENTRY POINT
   ================================================================ */
void oe_systeminfo_entry(void)
{
    pal_oe_info_t info;

    if (!pal_get_oe_info(&info))
    {
        ui_status(STATUS_ERROR);
        return;
    }

    /* Page 1 */
    ui_init();
    sysinfo_page1(&info);
    pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
    pal_print(yellow bold "[Page 1/3 - Press any key for next]" reset);
    pal_pause();

    /* Page 2 */
    ui_init();
    sysinfo_page2(&info);
    pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
    pal_print(yellow bold "[Page 2/3 - Press any key for next]" reset);
    pal_pause();

    /* Page 3 */
    ui_init();
    sysinfo_page3(&info);
    pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
    pal_print(yellow bold "[Page 3/3 - Press any key to return]" reset);
    pal_pause();
}