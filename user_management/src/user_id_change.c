/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: User Management
File: user_id_change.c
About: Original OE userid_change() converted
       to C using PAL and binary storage.
Revisions:
- 2026-02-21  Removed C++ system/iostream
- 2026-02-21  Converted to file abstraction
- 2026-03-16  Coordinates updated for 80x25 via ui_coordinates.h.
------------------------------------------------------------
*/

#include "pal.h"
#include "ui_setup.h"
#include "ui_elements.h"
#include "file.h"
#include "utils.h"
#include "user.h"

void userid_change(void)
{
    char newusername[USERNAME_MAX];

    ui_init();
    int row = UI_WIZARD_TITLE_ROW;
    int column = UI_WIZARD_TITLE_COL;
    ui_title(row++, column, underline RED bold,
        "Welcome to USER-ID Changing Wizard Ver 1.0.");

    ui_title(row++, column, yellow bold,
        "FOLLOW THE ONSCREEN INSTRUCTIONS TO CHANGE THE USER-ID");

    ui_title(row++, column, "", "Enter New USER-ID: ");

    ///util_get_string(nnn, USERNAME_MAX, "");
    pal_readline(newusername, USERNAME_MAX);
    /* Overwrite user file */
    file_write_string(USER_FILE, newusername);

    ui_title(row++, column, blue bold,
        "Re login to see the effect, press enter.");
    ui_status(STATUS_USERID_CHANGED);

    login();   /* original flow */
}