/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: Help Documentation
File: regedit_help.c
About: Displays help information for registry editor
Revisions:
- 2026-02-21  Converted legacy registry help to PAL-based system
- 2026-03-16  Coordinates updated for 80x25 via ui_coordinates.h.
- 2026-08-26  BUG FIX: missing pal_pause() at the end — every other
              help screen (prompt_help.c, settings_help.c) waits for
              a keypress before returning; this one fell straight
              through and got redrawn over immediately by the
              caller's next screen, so it was never actually
              readable.
------------------------------------------------------------
*/

#include "pal.h"
#include "help.h"
#include "ui_elements.h"
#include "ui_setup.h"
void help_show_regedit(void)
{
    //ui_title(33, 69, yellow bold, "start_admin_reg_edit -> Activate admin mode");
    int row = UI_HELP_TITLE_ROW;
    int column = UI_HELP_BODY_COL;
    pal_set_cursor(row++, column);
    pal_print(green bold "Welcome to HELP Documentation Ver: 1.0 :" yellow bold);
    pal_set_cursor(row++, column);
    pal_print("  install  -> Install all applications");
    pal_set_cursor(row++, column);
    pal_print("  temp     -> Temporary app access (TTT/quiz)");
    pal_set_cursor(row++, column);
    pal_print("  exit     -> Return to login");
    pal_set_cursor(row++, column);
    pal_print("  reset    -> Reset all apps");
    pal_set_cursor(row++, column);
    pal_print("  prompt   -> Go to main shell    ");
    pal_set_cursor(row++, column);
    pal_print("  help     -> Show this help    ");
    pal_set_cursor(row++, column);
    pal_print("  clear    -> Clear output screen.");
    pal_set_cursor(row++, column);
    pal_print("  user     -> Edit existing /add new user");

    pal_pause();
}