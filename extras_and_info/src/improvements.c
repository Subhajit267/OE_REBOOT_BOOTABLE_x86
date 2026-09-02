
/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-03-16
Module: Extras & Info
File: improvements.c
About: Displays list of improvements for current OE version.
       Rewritten for 80x25 grid with pagination (2 pages).
       Page 1: items i - viii
       Page 2: items ix - xiv + version footer

Revisions:
- 2026-02-21  Initial implementation
- 2026-03-10  Updated list for OE Version 8, branding integrated
- 2026-03-16  Rewritten for 80x25, paginated, ui_coordinates.h
- 2026-08-26  BUG FIX: footer wrapped OE_BUILD_TYPE in its own
              " (" ")" — but OE_BUILD_TYPE already includes its own
              parentheses, so the footer rendered as
              "OE 8.42.28 ((Pre Release Build C and PAL Based))".
              Removed the redundant wrapper.
------------------------------------------------------------
*/

#include "pal.h"
#include "ui_setup.h"
#include "ui_elements.h"
#include "extras.h"
#include "branding.h"

void extras_show_improvements(void)
{
    int row;

    /* ============================================================
       PAGE 1 — items i to viii
       ============================================================ */
    ui_init();
    row = UI_IMPROVE_TITLE_ROW;

    ui_title(row++, UI_IMPROVE_TITLE_COL, bold red underline,
        OE_NAME " - Improvements in Version " OE_VERSION);
    row++;

    ui_title(row++, UI_IMPROVE_BODY_COL, bold blue,
        "i.)   Complete Platform Abstraction Layer (PAL) introduced");
    ui_title(row++, UI_IMPROVE_BODY_COL, bold yellow,
        "ii.)  Cross-platform support for Windows and Linux");
    ui_title(row++, UI_IMPROVE_BODY_COL, bold blue,
        "iii.) New filesystem commands");
    ui_title(row++, UI_IMPROVE_BODY_COL, bold yellow,
        "iv.)  Binary file abstraction layer implemented");
    ui_title(row++, UI_IMPROVE_BODY_COL, bold blue,
        "v.)   Modular application installer system added");
    ui_title(row++, UI_IMPROVE_BODY_COL, bold yellow,
        "vi.)  Application registry system using .rg binary files");
    ui_title(row++, UI_IMPROVE_BODY_COL, bold blue,
        "vii.) New system information tool for hardware inspection");
    ui_title(row++, UI_IMPROVE_BODY_COL, bold yellow,
        "viii.) Built-in Notepad text editor introduced");

    pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
    pal_print(yellow bold "[Page 1/2 - Press any key for next]" reset);
    pal_pause();

    /* ============================================================
       PAGE 2 — items ix to xiv + footer
       ============================================================ */
    ui_init();
    row = UI_IMPROVE_TITLE_ROW;

    ui_title(row++, UI_IMPROVE_TITLE_COL, bold red underline,
        OE_NAME " - Improvements (continued)");
    row++;

    ui_title(row++, UI_IMPROVE_BODY_COL, bold blue,
        "ix.)  Advanced console UI layout and rendering engine");
    ui_title(row++, UI_IMPROVE_BODY_COL, bold yellow,
        "x.)   Centralized branding system for version and build info");
    ui_title(row++, UI_IMPROVE_BODY_COL, bold blue,
        "xi.)  Improved prompt shell with command based architecture");
    ui_title(row++, UI_IMPROVE_BODY_COL, bold yellow,
        "xii.) Enhanced user account management system");
    ui_title(row++, UI_IMPROVE_BODY_COL, bold blue,
        "xiii.) Modular source tree architecture for easier expansion");
    ui_title(row++, UI_IMPROVE_BODY_COL, bold yellow,
        "xiv.) Performance and stability improvements across modules");

    row++;
    ui_title(row++, UI_IMPROVE_BODY_COL, bold green,
        OE_SHORT_NAME " " OE_VERSION " " OE_BUILD_TYPE);
    ui_title(row++, UI_IMPROVE_BODY_COL, bold green,
        OE_COPYRIGHT " " OE_DEVELOPER);

    pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
    pal_print(yellow bold "[Page 2/2 - Press any key to return]" reset);
    pal_pause();
}