/*
------------------------------------------------------------
Author: Subhajit Halder
Module: User Management
File: login.c
About: Original OE login function – updated for 80x25 VGA grid.
Revisions:
- 2026-02-21  Replaced timerS() with util_timer()
- 2026-02-22  Fixed reg_status values, replaced reg_edit/source with regedit_run/extras_show_source
- 2026-02-23  Adjusted coordinates, added trim_whitespace for password comparison
- 2026-03-16  Coordinates updated for 80x25 via ui_coordinates.h.
              reg_status renamed to is_guest_mode.
- 2026-08-25  BUG FIX: guest-mode message was 71 visible chars printed
              at col 19, overflowing the 79-col right edge by ~10.
              Shortened to fit one line. Also, the stored username was
              printed with no length cap — anything longer than ~40
              chars (well within USERNAME_MAX=64) would overflow past
              col 79. Display now truncates via a bounded copy.
- 2026-08-26  SECURITY: password was stored and compared as plaintext
              (pwd.bd held it in the clear). Switched to salted
              SHA-256 via utilities/password_hash.c — pwd.bd now
              holds "saltHex:digestHex", verified with
              password_verify() instead of a raw pal_strcmp(). The
              "0" sentinel (no password set) is unaffected and still
              compared directly. Not backward compatible with old
              plaintext pwd.bd files — recreate the user via the
              installer.

ALL CHECKED AND WORKING
------------------------------------------------------------
*/

#include "pal.h"
#include "ui_setup.h"
#include "ui_elements.h"
#include "file.h"
#include "utils.h"
#include "user.h"
#include "regedit.h"
#include "extras.h"
#include "prompt.h"
#include "password_hash.h"

char current_user[USERNAME_MAX] = "";
int is_guest_mode = 0;

void login(void)
{
    char name1[USERNAME_MAX];
    char password1[PASSWORD_HASH_LEN];   /* holds the stored hash (or "0") */
    char password[PASSWORD_MAX];         /* holds what the user types */
    int row = UI_LOGIN_MSG_ROW;
    int col = UI_LOGIN_MSG_COL;

    ui_init();

    if (file_exists(USER_FILE))
    {
        is_guest_mode = 0;   /* normal user */

        ui_title(row, col, "",
            "  To begin press enter or type password.");

        ui_title(row + 3, col + 10, "", "User-ID:");
        file_read_string(USER_FILE, name1, USERNAME_MAX);
        {
            /* col+21 leaves 40 usable columns before the col-79 edge */
            char name_disp[41];
            pal_strncpy(name_disp, name1, sizeof(name_disp));
            ui_title(row + 3, col + 21, "", name_disp);
        }
        file_read_string(PASS_FILE, password1, PASSWORD_HASH_LEN);
        pal_strcpy(current_user, name1);
        //pal_set_cursor(row+33, col+21); pal_print(B4); pal_print(password1);pal_print( reset); /* debug: show stored password (remove in production) */
        if (pal_strcmp(password1, "0") == 0)
        {
            pal_pause();
            prompt();
        }
        else
        {
            ui_title(row + 4, col + 10, "", "Password:  ");
            pal_print(invisible);
            //util_get_string(password, PASSWORD_MAX, "");
            pal_readline(password, PASSWORD_MAX);
            pal_print(reset);
            //pal_set_cursor(row+31, col+22); pal_print(B4); pal_print(password); pal_print(reset); /* debug: show stored password (remove in production) */
            /* Trim both stored and entered passwords to avoid whitespace mismatches */
            trim_whitespace(password1);
            trim_whitespace(password);

            if (password_verify(password, password1))
                prompt();
            else if (pal_strcmp(password, "registryeditor") == 0)
                reg_edit();
            else if (pal_strcmp(password, "guess_it") == 0)
                extras_show_source();
            else
            {

                lockdown("password");
                //           ui_title(19, 69, RED bold,
                               //"  Incorrect password. ACCESS DENIED");
                //           ui_title(21, 69, RED bold, "  !!!!SYSTEM WILL SHUTDOWN!!!!");
                //           util_timer(10,22,71);
                //           pal_exit();
            }
        }
    }
    else
    {
        is_guest_mode = 1;
        ui_title(row + 1, col, "",
            "To just test the program (limited features), press enter.");
        ui_title(row + 3, col + 10, "", "USER-ID: GUEST");
        pal_pause();
        ui_init();
        prompt();
    }
}