/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-03-16
Module: System Tools
File: settings.c
About: Settings panel -- efficient menu-driven code,
       restored original OE UI with personalization menu
       and separate color change wizard (exact original table),
       base row = UI_SETTINGS_ROW, col = UI_SETTINGS_COL.
Revisions:
- 2026-02-21  Initial menu-based version
- 2026-02-22  Fixed user_accounts to use existing functions
- 2026-02-24  Base row 15, split personalization/color_change,
              reset & restore loops correctly,
              color change table now matches original OE exactly
- 2026-03-16  BASE_ROW/COL now use ui_coordinates.h constants.
              Tabs removed from ui_menu title strings.
              color_change() rewritten for 80x25:
                - 1 row per color, no gap rows
                - header at row 4, 16 colors rows 5-20,
                  input row 22, feedback via ui_status()
                - exactly fits 80x25 content zone
              reset_restore() warning row placement fixed.
              ui_menu() title no longer uses start_row-2.
- 2026-04-12  NEW 80*25 Layout implemented
- 2026-08-25  BUG FIX: color_change() drew an extra top-border row
              before the header that wasn't in the documented layout,
              pushing the whole table down by one row and leaving no
              room for the input prompt inside the content zone
              (rows 4-22). The input had been hardcoded to row 11/12,
              which sits in the middle of the color table. Removed
              the stray extra border line so header/colors/border
              land on the documented rows (4 / 5-20 / 21) and the
              input prompt now sits cleanly below the table at row 22.

//ALL CHECKED ALL WORKS
------------------------------------------------------------
*/

#include "pal.h"
#include "ui_setup.h"
#include "app_installer.h"
#include "user.h"
#include "help.h"
#include "settings.h"
#include "file.h"
#include "utils.h"
#include "prompt.h"
#include "installer.h"
#include "ui_elements.h"
/* Base coordinates for all settings panels */
#define BASE_ROW UI_SETTINGS_ROW
#define BASE_COL UI_SETTINGS_COL

#define LOOKS_FILE "looks.bd"

/* Forward declarations */
static void user_accounts(void);
static void reset_restore(void);
static void personalization(void);
static void color_change(void);

/* ================= USER ACCOUNTS ================= */

static void user_accounts(void)
{
    const char* options[] = {
        green bold "1.) Add /Remove /Create Password for your accounts",
        green bold "2.) Change Your User_ID.",
        green bold "3.) EXIT."
    };
    int choice;
    do
    {
        choice = ui_menu(BASE_ROW, BASE_COL,
            red bold underline "Welcome to User Account Control Panel:" reset, options, 3);
        if (choice == 1) password_change();
        else if (choice == 2) userid_change();
        if (choice > 3 || choice < 1) ui_status(STATUS_INVALID);
    } while (choice != 3);
}

///* ================= RESET & RESTORE ================= */
//static void reset_restore(void)
//{
//    char a[4];
//    int row = BASE_ROW, col = BASE_COL;
//
//    while (1)
//    {
//        ui_init();
//
//        ui_title(row, col, red bold underline, "Welcome to Reset and Restore Panel:");
//        ui_title(row + 1, col + 2, yellow bold, "Pick a task: ");
//        ui_title(row + 2, col + 4, blue bold, "1.) Reset your applications");
//        ui_title(row + 3, col + 4, blue bold, "2.) Restore your system by re-installing it.");
//        ui_title(row + 4, col + 4, blue bold, "3.) EXIT.");
//        ui_title(row + 5, col + 2, red bold,
//            "Warning!! Any of the above task(command) will reset all your applications thus delete /alter data. ");
//        //ui_title(row + 6, col + 2, green bold, "Do you want to continue(y/n)?");
//        //ui_confirm(row+6, col+2, "Do you want to continue");
//        //pal_set_cursor(row + 6, col + 32);
//        //pal_readline(a, sizeof(a));
//
//        if (ui_confirm(row + 6, col + 2, "Do you want to continue");)
//        {
//            ui_title(row + 7, col + 4, green bold, "Enter choice(1-3): ");
//            pal_set_cursor(row + 7, col + 26);
//            pal_readline(a, sizeof(a));
//
//            if (pal_strcmp(a, "1") == 0)
//            {
//                uninstall_all(1);
//                /* loop continues - back to reset panel */
//            }
//            else if (pal_strcmp(a, "2") == 0)
//            {
//                installer_prompt();   /* full installer - may not return */
//                /* if it returns, continue loop */
//            }
//            else if (pal_strcmp(a, "3") == 0)
//                return;   /* exit to main settings */
//            else
//                ui_status(STATUS_ERROR);
//        }
//        else
//            return;//ui_status(STATUS_ERROR);
//            //continue;   /* user chose not to continue */
//    }
//}

/* ================= RESET & RESTORE ================= */

static void reset_restore(void)
{
    const char* options[] = {
        green bold "1.) Reset your applications",
        green bold "2.) Restore your system by re-installing it.",
        green bold "3.) EXIT."
    };
    int choice;
    do
    {
        choice = ui_menu(BASE_ROW, BASE_COL,
            red bold underline "Welcome to Reset and Restore Panel:" reset, options, 3);

        if (choice == 1 || choice == 2)
        {
            /* ui_menu(BASE_ROW=5, count=3) draws:
               title=row5, options=row7-9, input=row11
               Warning shown at row13, confirm at row14 — within content zone */
            int warn_row = BASE_ROW + 3 + 5;   /* = 13 */
            ui_title(warn_row, BASE_COL, red bold,
                "Warning!! This will reset all applications and delete data.");
            if (!ui_confirm(warn_row + 1, BASE_COL, "Do you want to continue"))
                continue;
        }
        if (choice == 1) uninstall_all(1);
        else if (choice == 2) installer_prompt();
        if (choice > 3 || choice < 1) ui_status(STATUS_INVALID);
    } while (choice != 3);
}

/* ================= COLOR CHANGE WIZARD ================= */
/*
  80x25 layout — exactly fits content zone (rows 4-22):
  Row  4 : column header  "| COLOR_NAME     | SAMPLE |"
  Row  5 : color 1  (Red)
  Row  6 : color 2  (Green)
  ...
  Row 20 : color 16 (Blank)
  Row 21 : bottom border
  Row 22 : "Enter choice (To exit press e):"
  Row 23 : status bar — feedback via ui_status()

  No gap rows. No title row inside function (context clear from menu).
  All 16 colors visible at once — no pagination needed.
*/
static void color_change(void)
{
    char input[4];
    int r, c;

    /* Color names — index i*2 = name, odd indices = spacer (unused) */
    const char* color_names[] = {
        "Red",          " ", "Green",        " ", "Yellow",      " ",
        "Blue",         " ", "Purple",        " ", "cyan",        " ",
        "Light Grey",   " ", "Grey",          "",  "Light Red",   " ",
        "Light Green",  " ", "Light Yellow",  " ", "Light Blue",  " ",
        "Light Purple", " ", "Light cyan",    "",  "White",       " ",
        "Blank",        " "
    };

    const char* color_samples[] = {
        B1,  B2,  B3,  B4,  B5,  B6,  B7,  B8,
        B9,  B10, B11, B12, B13, B14, B15, B16
    };

    int num_colors = 16;

    while (1)
    {
        ui_init();

        r = 4;                   /* start at row 4 — just below build type */
        c = UI_COLOR_TABLE_COL;  /* = 19 */
        /* Column header row */
        ui_title(r, c, "", "| ");
        ui_title(r, c + 2, green bold, "COLOR_NAME     ");
        ui_title(r, c + 18, "", "| ");
        ui_title(r, c + 20, green bold, "SAMPLE");
        ui_title(r, c + 27, "", "|");
        r++;   /* r = 5 */

        /* 16 color rows — 1 row per color */
        for (int i = 0; i < num_colors; i++)
        {
            char left[32];
            char num_buf[4];

            pal_strcpy(left, "| ");
            pal_itoa(i + 1, num_buf);
            pal_strcat(left, num_buf);
            pal_strcat(left, ".) ");
            pal_strcat(left, color_names[i * 2]);

            ui_title(r, c, "", left);        /* "| N.) ColorName" */
            pal_set_cursor(r, c + 18);
            pal_print("| ");
            pal_set_cursor(r, c + 20);
            pal_print(color_samples[i]);
            pal_print("     ");                   /* sample block */
            pal_print(reset);
            pal_set_cursor(r, c + 27);
            pal_print("|");

            r++;   /* r = 5+i+1, ends at r=21 after 16 iterations */
        }

        /* Bottom border at row 21 */
        ui_title(r++, c, "", " --------------------------");

        /* Input at row 22 — directly below the table, on the same row
           as its own label since row 23 is reserved for ui_status(). */
        {
            const char* prompt_text = "Enter choice (To exit press e): ";
            ui_title(r, c, purple bold, prompt_text);
            pal_set_cursor(r, c + (int)pal_strlen(prompt_text));
        }
        pal_readline(input, sizeof(input));

        if (pal_strcmp(input, "e") == 0 || pal_strcmp(input, "E") == 0)
            return;

        int num = pal_atoi(input);

        if (num >= 1 && num <= 15)
        {
            file_write_int(LOOKS_FILE, num);
            ui_status(STATUS_SUCCESS);   /* "Color changed. Restart OE to see effect." */
            return;
        }
        else if (num == 16)
        {
            file_write_int(LOOKS_FILE, 16);
            ui_status(STATUS_SUCCESS);
            return;
        }
        else
        {
            ui_status(STATUS_INVALID);
            /* loop continues - redraw table */
        }
    }
}

/* ================= PERSONALIZATION (menu) ================= */

static void personalization(void)
{
    const char* options[] = {
        green bold "1.) Change your border color.",
        green bold "2.) EXIT."
    };
    int choice;
    do
    {
        choice = ui_menu(BASE_ROW, BASE_COL,
            red bold underline "Welcome to Personalization Panel:" reset, options, 2);
        if (choice == 1) color_change();
        if (choice > 2 || choice < 1) ui_status(STATUS_ERROR);
    } while (choice != 2);
}

/* ================= HELP (in settings) ================= */

static void settings_help(void)
{
    help_show_settings();
}

/* ================= MAIN SETTINGS LOOP ================= */

void settings_run(void)
{
    //color_change();   /* first run - direct to color change wizard */
    const char* options[] = {
        green bold "1.) USER ACCOUNTS",
        green bold "2.) RESET AND RESTORE",
        green bold "3.) PERSONALIZATION",
        green bold "4.) HELP",
        green bold "5.) EXIT"
    };
    int choice;

    do
    {
        //pal_set_cursor(35, BASE_COL);
        //pal_print(yellow "Available sub-categories:");
        choice = ui_menu(BASE_ROW, BASE_COL,
            red underline "Welcome to the Settings" reset, options, 5);

        switch (choice)
        {
        case 1: user_accounts();  break;
        case 2: reset_restore();  break;
        case 3: personalization(); break;
        case 4: settings_help();  break;
        case 5: return;
        default:
            /* Hidden shortcuts */
            if (choice == 7)
                color_change();   /* direct to color change */
            else if (choice == 8)
                add_user();
            else
                ui_status(STATUS_INVALID);
        }
    } while (choice != 5);
}