/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: Help Documentation
File: settings_help.c
About: Displays help information for settings module
Revisions:
- 2026-02-21  Converted legacy settings help to PAL-based layout
- 2026-03-16  Coordinates updated for 80x25 via ui_coordinates.h.
- 2026-08-25  BUG FIX: item was labelled "4.)" but told the user to
              press 5 (EXIT is option 5 in settings_run(), option 4
              is this Help screen itself). Relabelled to "5.)" so the
              printed number matches the key to press.
------------------------------------------------------------
*/

#include "pal.h"
#include "help.h"
#include "ui_elements.h"
void help_show_settings(void)
{
    int row = UI_HELP_TITLE_ROW+8;
    int column = UI_HELP_BODY_COL;
    pal_set_cursor(row++, column);
    pal_print(green bold "Welcome to Help Documentation Ver: 1.0" reset yellow bold);
    pal_set_cursor(row++, column);
    pal_print("1.) Press 1 to Modify User Credentials.");
    pal_set_cursor(row++, column);
    pal_print("2.) Press 2 to reset or restore your account.");
    pal_set_cursor(row++, column);
    pal_print("3.) Press 3 to personalize account by changing colors.");
    pal_set_cursor(row++, column);
    pal_print("5.) Press 5 to exit settings.");

    pal_pause();
}