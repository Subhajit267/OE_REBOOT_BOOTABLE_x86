/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: UI
File: bootscreen.c
About: Classic OE bootscreen (identical look).
Revisions:
- 2026-02-21  Rewritten using ui_setup + PAL
- 2026-08-26  BUG FIX: app-title/version lines were indented with
              4 literal '\t' characters instead of an explicit
              column. Tab-stop width is terminal-dependent, so the
              text could drift right of the documented "content
              always col 19" grid depending on the terminal. Replaced
              with a fixed numeric column offset.

ALL CHECKED AND WORKING
------------------------------------------------------------
*/

#include "pal.h"
#include "ui_setup.h"
#include "ui_elements.h"
#include "bootscreen.h"

void bootscreen_show(int app_id)
{
    ui_init();
    pal_print(green bold); 
    int row = UI_BOOT_APP_ROW;
    int column = UI_BOOT_APP_COL;
    int title_col = column + 20;   /* fixed indent, was tab-dependent */
    if (app_id == 1)
    {
        ui_title(row++, title_col, underline, "TIC TAC TOE");
        ui_title(row++, title_col, yellow bold, "Version:4.0");
    }
    else if (app_id == 2)
    {
        ui_title(row++, title_col, underline, "QUIZ__APP");
        ui_title(row++, title_col, yellow bold, "Version:10.8");
    }
    else if (app_id == 3)
    {
        ui_title(row++, title_col, underline, "TEMP_CONV");
        ui_title(row++, title_col, yellow bold, "Version:4.05");
    }
    //else if (app_id == 4)
    //{
    //    ui_title(row++, column, underline, "\t\t\t\t   CALCULATOR   ");
    //    ui_title(row++, column, yellow bold, "\t\t\t\t    Version:3.05");
    //}
    else if (app_id == 5)
    {
        ui_title(row++, title_col, underline, "EQN_SOLVER");
        ui_title(row++, title_col, yellow bold, "Version:3.05");
    }
    else if (app_id == 6)
    {
        ui_title(row++, title_col, underline, "QUAD_EQN_S");
        ui_title(row++, title_col, yellow bold, "Version:3.05");
    }
    else if (app_id == 7)
    {
        ui_title(row++, title_col, underline, "Ar(T)CALC");
        ui_title(row++, title_col, yellow bold, "Version:3.05");
    }
    else if (app_id == 0)
        ;
    else
        ui_status(STATUS_ERROR);

    ui_title(UI_BOOT_WAIT_ROW, UI_BOOT_WAIT_COL, cyan bold, "Please_Wait.");
    ui_title(UI_BOOT_LOAD_ROW, UI_BOOT_LOAD_COL, yellow bold, "LOADING......");

    progressbar(UI_PROGRESS_ROW, UI_PROGRESS_COL, UI_PROGRESS_WIDTH);

    pal_print(reset);
}