/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: Utilities
File: timer.c
About: Countdown timer using PAL abstraction.
Revisions:
- 2026-02-21  Initial implementation
- 2026-02-21  Converted to PAL-only backend
- 2026-03-16  lockdown() coordinates updated for 80x25 via ui_coordinates.h.
------------------------------------------------------------
*/

#include "utils.h"
#include "pal.h"
#include "ui_setup.h"
#include "ui_elements.h"
void util_timer(int seconds, int row, int column)
{
    int t = seconds;

    while (t >= 0)
    {
        pal_set_cursor(row, column);
        pal_print(red bold "\a\a\aTIME LEFT: ");

        char buf[12];
        int i = 0;

        if (t == 0)
            buf[i++] = '0';
        else
        {
            int tmp = t;
            char rev[12];
            int j = 0;

            while (tmp > 0)
            {
                rev[j++] = (tmp % 10) + '0';
                tmp /= 10;
            }

            while (j > 0)
                buf[i++] = rev[--j];
        }

        buf[i] = '\0';

        pal_print(buf);
        pal_print("   ");

        pal_sleep(1);
        t--;
    }

}

void lockdown(char* string)
{

    int row = UI_ROW_AFTER_LOGO;   /* = 11 */
    int col = UI_COL_LOGO;           /* = 19 */
    ui_title(row, col, RED bold,
        "  Incorrect ");
    pal_print(red bold);
    pal_print(string);
	ui_status(STATUS_ACCESS_DENIED);
    ui_title(row + 2, col, RED bold, "  !!!!SYSTEM WILL SHUTDOWN!!!!");
    util_timer(10, ++row, col + 2);
    pal_exit();
}