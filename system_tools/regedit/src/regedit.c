/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-24
Module: System Tools
File: regedit.c
About: Registry Editor -> fully command‑based, brief description,
       old admin command restored,
       and persistent ADMIN MODE indicator.
Revisions:
- 2026-02-21  Converted to ui_menu based system
- 2026-02-21  Removed command shell style
- 2026-02-22  Added admin mode with separate commands
- 2026-02-23  Corrected command table and handlers
- 2026-02-24  Adjusted row coordinates, added admin mode indicator,
               and restored original OE command set (with temp mode)
- 2026-02-24  Finalized command handlers and table, all commands checked and working
- 2026-03-16  Coordinates updated for 80x25 via ui_coordinates.h.
              col_base+70 overflow fixed (admin indicator moved to next row).
              reg_status renamed to is_guest_mode.
//ALL CHECKED AND WORKING
------------------------------------------------------------
*/


// reset and add user to be merged
// add key and temp to be merged
#include "pal.h"
#include "ui_setup.h"
#include "ui_elements.h"
#include "user.h"
#include "app_installer.h"
#include "help.h"
#include "regedit.h"
#include "utils.h"
#include "prompt.h"

int reg_admin_mode = 0;   /* 0 = normal, 1 = admin */

int row_base = UI_REGEDIT_ROW;
int col_base = UI_REGEDIT_COL;
/* ================= COMMAND HANDLERS ================= */

static void cmd_install(void) { install_all(); }

static void cmd_temp(void)
{
    char input[32];
    ui_title(row_base + 11, col_base, "", "Enter App (TTT / quiz): ");
    pal_set_cursor(row_base + 11, col_base + 25);
    pal_readline(input, sizeof(input));

    if (pal_strcmp(input, "TTT") == 0 || pal_strcmp(input, "ttt") == 0)
    {
        if (reg_admin_mode)
        {
            ui_title(row_base + 12, col_base, green bold, "Launching TicTacToe (temp mode)...");
            pal_pause();
        }
        else
            ui_status(STATUS_INVALID);
    }
    else if (pal_strcmp(input, "quiz") == 0)
    {
        if (reg_admin_mode)
            ui_title(row_base + 12, col_base, green bold, "Launching Quiz (temp mode)...");
        else
            ui_title(row_base + 12, col_base, yellow bold, "Launching Quiz in trial mode...");
        pal_pause();
    }
    else
    {
        ui_title(row_base + 12, col_base, red bold, "temp command is available only for TTT or quiz.");
        pal_pause();
    }
}

static void cmd_exit(void) { ui_init(); /*layout(); logo(); */login(); }

static void cmd_reset(void)
{
    if (ui_confirm(row_base + 11, col_base, "Warning! This will reset all applications. Continue?"))
        uninstall_all(1);
}

static void cmd_prompt(void) { prompt(); }

static void cmd_add_key(void)
{
    if (!reg_admin_mode) { ui_status(STATUS_INVALID); return; }
    char input[32];
    ui_title(row_base + 11, col_base, "", "Enter app (both / quiz / tictactoe): ");
    pal_set_cursor(row_base + 11, col_base + 36);
    pal_readline(input, sizeof(input));

    if (pal_strcmp(input, "both") == 0)
    {
        install_app(find_app("tictactoe"));
        install_app(find_app("quiz"));
        ui_status(STATUS_SUCCESS);
    }
    else if (pal_strcmp(input, "quiz") == 0)
    {
        install_app(find_app("quiz"));
        ui_status(STATUS_SUCCESS);
    }
    else if (pal_strcmp(input, "tictactoe") == 0)
    {
        install_app(find_app("tictactoe"));
        ui_status(STATUS_SUCCESS);
    }
    else ui_status(STATUS_INVALID);
}

static void cmd_user(void)
{
    if (!reg_admin_mode) { ui_status(STATUS_INVALID); return; }
    if (ui_confirm(row_base + 12, col_base, "Warning! This will reset user account. Continue?"))
        add_user();
}

static void cmd_help(void)
{
    help_show_regedit();
    pal_pause();
}

static void cmd_start_admin(void)
{
    if (is_guest_mode)
    {
        reg_admin_mode = 0;
        ui_title(row_base + 14, col_base, RED bold, "GUEST USER NOT ALLOWED");
    }
    else
    {
        reg_admin_mode = 1;
        ui_title(row_base + 14, col_base, green bold, "Admin mode activated!");
    }
    pal_pause();
}

/* Shared handler for clear/cls -> does nothing, as the loop already redraws the screen */
static void cmd_clear(void) { /* Nothing needed -> screen will redraw on next iteration */ }

/* ================= COMMAND TABLE ================= */

typedef struct {
    const char* name;          /* full command name (lowercase) */
    void (*handler)(void);      /* function to call */
    int need_admin;             /* 1 if admin mode required */
} cmd_entry_t;

static const cmd_entry_t cmd_table[] = {
    { "install", cmd_install, 0 },
    { "temp",    cmd_temp,    0 },
    { "exit",    cmd_exit,    0 },
    { "reset",   cmd_reset,   0 },
    { "prompt",  cmd_prompt,  0 },
    { "add_key", cmd_add_key, 1 },
    { "user",    cmd_user,    1 },
    { "help",    cmd_help,    0 },
    { "start_admin_reg_edit", cmd_start_admin, 0 },
    { "clear",   cmd_clear,   0 },   /* shared handler */
    { "cls",     cmd_clear,   0 },   /* same handler */
    { NULL, NULL, 0 }
};

/* ================= MAIN LOOP ================= */

void reg_edit(void)
{
    char cmd_raw[25], cmd_lower[25];

    while (1)
    {
        ui_init();
        /* Title with optional admin indicator */
        ui_title(row_base, col_base, bold underline red,
            "Welcome to Registry Editor version:3.5.0(For Power Users).");
        /* Admin indicator on next row — col_base+70 overflows 80-col screen */
        if (reg_admin_mode)
            ui_title(row_base + 1, col_base, bold RED, "#### ADMIN MODE ####");

        ui_title(row_base + 2, col_base, green bold, "Things you can do:");
        ui_title(row_base + 3, col_base + 2, "", "* Install/uninstall applications");
        ui_title(row_base + 4, col_base + 2, "", "* Temporarily run TicTacToe or Quiz");
        ui_title(row_base + 5, col_base + 2, "", "* Reset all applications");
        ui_title(row_base + 6, col_base + 2, "", "* Manage user accounts");
        ui_title(row_base + 7, col_base + 2, "", "* Add keys for TTT/Quiz (admin)");
        ui_title(row_base + 8, col_base + 2, cyan bold, "Type 'help' for command list.");

        ui_title(row_base + 10, col_base, blue bold, "Command->  ");
        pal_set_cursor(row_base + 10, UI_REGEDIT_INPUT_COL);
        pal_readline(cmd_raw, sizeof(cmd_raw));

        pal_strcpy(cmd_lower, cmd_raw);
        for (int i = 0; cmd_lower[i]; i++)
            if (cmd_lower[i] >= 'A' && cmd_lower[i] <= 'Z')
                cmd_lower[i] += 'a' - 'A';

        /* Hidden code 69 -> also activates admin mode */
        //if (pal_strcmp(cmd_lower, "69") == 0) { cmd_start_admin(); pal_pause(); continue; }

        int found = 0;
        for (int i = 0; cmd_table[i].name != NULL; i++)
        {
            if (pal_strcmp(cmd_lower, cmd_table[i].name) == 0)
            {
                found = 1;
                if (cmd_table[i].need_admin && !reg_admin_mode)
                    ui_status(STATUS_INVALID);
                else if (cmd_table[i].handler)
                    cmd_table[i].handler();
                //pal_getchar();  /* Wait for key press before refreshing screen */
                break;
            }
        }
        if (!found)
            ui_status(STATUS_INVALID);
    }
}