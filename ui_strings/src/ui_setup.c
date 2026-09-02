/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: UI Setup
File: ui_setup.c
About: Structured UI interaction utilities for OE_REBOOT.
Revisions:
- 2026-02-21  Initial implementation
- 2026-02-21  Removed stdlib & stdio usage
- 2026-02-21  Replaced atoi & snprintf with PAL versions
- 2026-03-16  ui_status() row/col updated to UI_STATUS_ROW/COL for 80x25.
- 2026-08-25  BUG FIX: STATUS_ALREADY_INSTALLED had no case in ui_status(),
              so it fell through to default and showed the scary
              "SYSTEM HALTED" message. Added its own case.
------------------------------------------------------------
*/

#include "pal.h"
#include "ui_elements.h"
#include "ui_setup.h"

/* ================= SCREEN INIT ================= */

void ui_init(void)
{
    pal_clear_screen();
    layout();
    logo();
}

/* ================= TITLE ================= */

void ui_title(int row, int col, const char* style, const char* text)
{
    pal_set_cursor(row, col);
    pal_print(style);
    pal_print(text);
    pal_print(reset);
}

/* ================= INTEGER PRINT ================= */

void ui_print_int(int value)
{
    char buffer[16];
    pal_itoa(value, buffer);
    pal_print(buffer);
}

void ui_title_int(int row, int col, const char* style, int value)
{
    char buffer[16];
    pal_itoa(value, buffer);
    ui_title(row, col, style, buffer);
}

/* ================= STATUS ================= */

static void print_status(const char* style, const char* message)
{
    ui_title(UI_STATUS_ROW, UI_STATUS_COL, style, message);
    pal_pause();  /* Wait for key press before clearing status */
    ui_title(UI_STATUS_ROW, UI_STATUS_COL,red , "                                                             ");
    //pal_sleep(1);
    ///* Clear status line */
    //ui_title(22, 5, "", "                                                    ");
}

void ui_status(ui_status_code code)
{
    switch (code)
    {
    case STATUS_SUCCESS:
        print_status(blue bold,
            "Successfully completed the command /operation.");
        break;

    case STATUS_ERROR:
        print_status(red bold,
            "No suitable record found.");
        break;

    case STATUS_WARNING:
        print_status(yellow bold,
            "Warning! Operation may alter system state.");
        break;

    case STATUS_INFO:
        print_status(cyan bold,
            "Information: Operation executed.");
        break;

    case STATUS_INVALID:
        print_status(red bold,
            "Invalid input detected /No such command.");
        break;

    case STATUS_NOT_INSTALLED:
        print_status(RED bold,
            "Application not installed.");
        break;

    case STATUS_INVALID_NUMBER_DECIMAL_FORMAT:
        print_status(red bold,
            "Invalid number /decimal format.");
        break;
    case STATUS_VALUUE_OUT_OF_RANGE:
        print_status(red bold,
            "Value out of range.");
        break;
    case STATUS_SYNTAX_ERROR:
        print_status(red bold,
            "Syntax Error: Invalid expression.");
        break;

    case STATUS_DIVISION_BY_ZERO:
        print_status(red bold,
            "Math Error: Division by zero.");
        break;

    case STATUS_ACCESS_DENIED:
        print_status(red bold,
            "Access Denied: Insufficient permissions.");
		break;
    case STATUS_USERID_CHANGED:
        print_status(GREEN bold,
            "Your USER-ID has been changed. ");
        break;
    case STATUS_ALREADY_INSTALLED:
        print_status(yellow bold,
            "Application is already installed.");
        break;
    default:////caution
        print_status(yellow bold,
            "SYSTEM HALTED FOR SECURITY REASONS");
        break;
    }
}

/* ================= MENU ================= */

int ui_menu(
    int start_row,
    int col,
    const char* title,
    const char* options[],
    int count
)
{
    char input[10];

    ui_init();

    ui_title(start_row, col, bold, title);

    for (int i = 0; i < count; i++)
    {
        ui_title(start_row + 2 + i, col, "", options[i]);
    }

    ui_title(start_row + count + 3, col, "", blue bold "Enter choice: ");
    pal_readline(input, sizeof(input));

    return pal_atoi(input);
}

/* ================= CONFIRM ================= */

int ui_confirm(int row, int col, const char* message)
{
    char input[10];

    ui_title(row, col, yellow bold, message);
    pal_print(" (y/n): ");

    pal_readline(input, sizeof(input));

    return (input[0] == 'y' || input[0] == 'Y');
}

/*
*
*
*
*/