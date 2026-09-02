/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: UI Setup
File: ui_setup.h
About: Unified UI interaction layer for OE_REBOOT.
       Provides menu, status, title, confirmation,
       and formatted integer printing.
Revisions:
- 2026-02-21  Initial modular UI interface
- 2026-02-21  Standardized status handling
------------------------------------------------------------
*/

#ifndef UI_SETUP_H
#define UI_SETUP_H

/* ================= STATUS CODES ================= */

typedef enum
{
    STATUS_SUCCESS = 0,
    STATUS_ERROR,
    STATUS_WARNING,
    STATUS_INFO,
    STATUS_INVALID,
    STATUS_NOT_INSTALLED,
	STATUS_INVALID_NUMBER_DECIMAL_FORMAT,
	STATUS_VALUUE_OUT_OF_RANGE,
    STATUS_SYNTAX_ERROR,
    STATUS_DIVISION_BY_ZERO,
	STATUS_ALREADY_INSTALLED,
	STATUS_ACCESS_DENIED,
    STATUS_USERID_CHANGED
} ui_status_code;

/* ================= CORE ================= */

void ui_init(void);

/* ================= TITLE PRINT ================= */

/*
Print styled text at specific location.
row, col are 1-based.
*/
void ui_title(int row, int col, const char* style, const char* text);

/* ================= INTEGER PRINT ================= */

void ui_print_int(int value);

void ui_title_int(int row, int col, const char* style, int value);

/* ================= STATUS ================= */

void ui_status(ui_status_code code);

/* ================= MENU ================= */

int ui_menu(
    int start_row,
    int col,
    const char* title,
    const char* options[],
    int count
);

/* ================= CONFIRM ================= */

int ui_confirm(int row, int col, const char* message);

#endif