/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-23
Module: Installer
File: installer.c
About: Classic OE installer flow – original strings restored,
       invisible input, backdoor commands preserved.
Revisions:
- 2026-02-21  Integrated user module + app_installer
- 2026-02-22  Added user_enter_guest() for trial, fixed app_id 0
- 2026-02-23  Restored original OE texts, made input invisible
- 2026-03-16  Coordinates updated for 80x25 via ui_coordinates.h.
              Off-screen column2=118 lines removed.
              reg_status renamed to is_guest_mode.
- 2026-08-25  BUG FIX: trial-mode message was 79 visible chars printed
              at col 19 (UI_INST_CHOICE_COL), overflowing the 79-col
              right edge by ~18. Split across two lines.

ALL CHECKED AND WORKING
------------------------------------------------------------
*/

#include "pal.h"
#include "ui_setup.h"
#include "ui_elements.h"
#include "app_installer.h"
#include "user.h"
#include "extras.h"
#include "installer.h"
#include "regedit.h"
#include "prompt.h"

/* Body of the old "[Y] Install" choice, factored out so bare metal can jump
   straight here (see installer_prompt() below) without showing the Y/N menu
   at all -- on bare metal, reaching this screen with no user.bd already
   means "this disk was just freshly formatted by installer.bin's own
   disk-installer (Stage 5) and needs its first account," there's no real
   ambiguity left to ask the user about. Hosted Windows/Linux builds still
   reach this only via the 'Y' choice below, unchanged. */
static void run_installation(void)
{
    int row = UI_INST_BODY_ROW;
    int col = UI_INST_BODY_COL;

    ui_init();

    ui_title(row++, col,
        underline red bold,
        "Preparing Operating Environment...");

    ui_title(row++, col,
        yellow bold,
        "Setup is configuring the system.");

    row++;

    ui_title(row++, col,
        yellow bold,
        "After installation:");

    ui_title(row++, col,
        yellow bold,
        "    Log on");

    ui_title(row++, col,
        yellow bold,
        "    Type HELP");

    ui_title(row++, col,
        yellow bold,
        "    Install applications");

    row++;

    ui_title(row++, col,
        cyan bold,
        "System initialization will continue.");
    /* column2=118 lines removed — off-screen on 80-col display */

    progressbar(UI_PROGRESS_ROW, UI_PROGRESS_COL, UI_PROGRESS_WIDTH);
    ///if (!user_exists())
    add_user();

    //if (app_id != 0)
    //{
    //    const char* app_name = NULL;
    //    switch (app_id)
    //    {
    //    case 1: app_name = "tictactoe"; break;
    //    case 2: app_name = "quiz"; break;
    //    case 3: app_name = "temp_conv"; break;
    //    case 4: app_name = "calculator"; break;
    //    default: break;
    //    }
    //    if (app_name)
    //    {
    //        const app_t* app = find_app(app_name);
    //        if (app) install_app(app);
    //    }
    //}
    /* Create all app registry files with value 0 (uninstalled) */
    uninstall_all(0); // flag set to 0 to supress "Uninstalled" status messages
    extras_show_improvements();

    if (pal_is_bare_metal())
    {
        /* On bare metal, finish first-time setup with a real reboot
           instead of falling into login() in the same session --
           the next boot naturally lands on login() itself since
           user_exists() is true by then (kernel_main() in
           kernel/src/kernel.c). Hosted Windows/Linux builds have no
           real "next boot" to hand off to, so they keep flowing
           straight into login() below exactly as before -- this
           branch never runs there (pal_is_bare_metal() is 0). */
        ui_init();
        ui_title(UI_INST_BODY_ROW, UI_INST_BODY_COL, green bold,
            "Setup complete. Restarting...");
        pal_sleep(1.5);
        pal_reboot();
    }
    login();
}

void installer_prompt()
{
    /* Bare metal never shows the Y/N wizard: the only way kernel_main()
       gets here with no user.bd is a disk installer.bin (Stage 5) just
       finished formatting this disk and handed off to it, or a genuinely
       fresh/blank disk booted straight via the Stage-4 bootloader -- either
       way "install" was already decided, so ask for the first account
       directly instead of asking the user to choose Install again. Hosted
       Windows/Linux builds keep the original menu (there's no separate
       disk-install step for them to have already gone through). */
    if (pal_is_bare_metal())
    {
        run_installation();
        return;
    }

    while (1)
    {
        char input[32];

        ui_init();

        int row = UI_INST_BODY_ROW;   /* base coordinates */
        int col = UI_INST_BODY_COL;

        /* ===== Main Installer Screen ===== */
        ui_title(row++, col+6, RED bold underline,
            "Operating Environment Setup Wizard");
        ui_title(row++, col, BLUE bold,
            "Operating Environment can be installed");

        ui_title(row++, col, BLUE bold,
            "now, or started in Guest Mode without");

        ui_title(row++, col, BLUE bold,
            "making changes to your system.");

        row++;

        ui_title(row++, col + 12,
            yellow bold underline,
            "Choose an option:" reset);

        ui_title(row++, col, white bold,
            "[Y] Install");

        ui_title(row++, col, green bold,
            "    Install Operating Environment.");

        ui_title(row++, col, white bold,
            "[N] Guest Mode");

        ui_title(row++, col, green bold,
            "    Start without installation.");

        ui_title(row++, col, purple bold,
            "Available commands during Guest Mode:");

        ui_title(row++, col, cyan bold,
            "      LOGIN");

        ui_title(row++, col, cyan bold,
            "      INSTALL");

        ui_title(row++, col, cyan bold,
            "      EXIT");

        ui_title(row++, col, red bold,
            "Select an option (Y/N):");

        /* Invisible input (like original cout<<invisible) */
        pal_print(invisible);
        pal_readline(input, sizeof(input));
        pal_print(reset);

        /* ===== Trial Mode ===== */
        if (pal_strcmp(input, "n") == 0 || pal_strcmp(input, "N") == 0)
        {
            ui_title(UI_INST_CHOICE_ROW, UI_INST_CHOICE_COL, "",
                "After completion of your trial, type 'install'");
            ui_title(UI_INST_CHOICE_ROW + 1, UI_INST_CHOICE_COL, "",
                "to restart the setup and continue.");
            pal_pause();
            login();
            user_enter_guest();
            return;
        }
        /* ===== Registry Editor Admin Backdoor ===== */
        else if (pal_strcmp(input, "registryeditor_admin") == 0)
        {
            reg_admin_mode = 1;   /* set guest mode to admin */
            reg_edit();
            return;
        }
        else if (pal_strcmp(input, "registryeditor") == 0)
        {
            reg_admin_mode = 0;   /* set guest mode to regular */
            reg_edit();
            return;
        }
        /* ===== Prompt Backdoor (skip setup) ===== */
        else if (pal_strcmp(input, "prompt") == 0)
        {
            is_guest_mode = 0;
            prompt();
            return;
        }
        /* ===== Direct Install ===== */
        else if (pal_strcmp(input, "y") == 0 || pal_strcmp(input, "Y") == 0)
        {
            run_installation();
        }
        /* ===== Invalid Input ===== */
        else
        {
            ui_status(STATUS_INVALID);
            continue;
        }
    }
}