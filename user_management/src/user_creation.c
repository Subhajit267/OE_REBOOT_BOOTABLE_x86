/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-23
Module: User Management
File: user_creation.c
About: Original OE add_user() – now validates inputs,
       rejects empty user ID, and prevents password "0".
Revisions:
- 2026-02-21  Converted to PAL + binary storage
- 2026-02-22  Added user_exists()
- 2026-02-23  Added input validation for empty user ID and password
- 2026-03-16  Coordinates updated for 80x25 via ui_coordinates.h.
- 2026-08-25  BUG FIX: the password retry loop never called ui_init()
              between attempts, so "row" grew by 1-2 on every failed
              attempt with no bound — same class of bug as
              password_management.c's retry loop. Now redraws the
              same two rows every attempt instead of advancing.
- 2026-08-26  SECURITY: password was written to pwd.bd as plaintext.
              Now hashed (salted SHA-256) via
              utilities/password_hash.c before being written.
------------------------------------------------------------
*/

#include "pal.h"
#include "ui_setup.h"
#include "ui_elements.h"
#include "file.h"
#include "utils.h"
#include "user.h"
#include "password_hash.h"

void add_user(void)
{
    char id[USERNAME_MAX];
    char c[8];
    char pass[PASSWORD_MAX];

    while (1) {
        int row = UI_WIZARD_TITLE_ROW;
        int col = UI_WIZARD_TITLE_COL;
        ui_init();
        ui_title(row++, col, underline RED bold, "Welcome to USER-ID Password Creation Wizard Ver: 1.7.9");
        ui_title(row++, col, "", "FOLLOW THE ONSCREEN INSTRUCTIONS TO CREATE A USER");
        ui_title(row++, col, yellow, "Please create a user id password for safety reasons:");

        ui_title(row++, col, "", "User-Id:");
        //util_get_string_echo(id, USERNAME_MAX, "");
        pal_readline(id, sizeof(id));
        //trim_whitespace(id);

        if (id[0] == '\0') {
            ui_title(row++, col, RED bold, "User-ID cannot be empty. Press Enter to try again.");
            pal_pause();
            continue;
        }

        file_write_string(USER_FILE, id);

        ui_title(row++, col, "", "Do you want to create a password(y/n):");
        pal_readline(c, sizeof(c));

        if (c[0] == 'y' || c[0] == 'Y')
        {
            /* Redraw the same two rows on every retry instead of
               advancing "row" — unbounded retries must not walk
               printed text past the content zone. */
            int prompt_row = row;
            int error_row = row + 1;

            while (1) {
                ui_title(prompt_row, col, "", "Enter password:                          ");
                //util_get_string(pass, PASSWORD_MAX, "");
                pal_readline(pass, PASSWORD_MAX);
                trim_whitespace(pass);
                //pal_readline(pass, sizeof(pass));
                if (pass[0] == '\0') {
                    ui_title(error_row, col, RED bold, "Password cannot be empty. Try again.     ");
                    pal_pause();
                    continue;
                }
                if (pal_strcmp(pass, "0") == 0) {
                    ui_title(error_row, col, RED bold, "The password cannot be 0. Choose another.");
                    pal_pause();
                    continue;
                }
                break;
            }
            row = error_row + 1;
            {
                char hashed[PASSWORD_HASH_LEN];
                password_hash(pass, hashed);
                file_write_string(PASS_FILE, hashed);
            }
        }
        else
        {
            file_write_string(PASS_FILE, "0");
        }

        ui_title(row + 4, col, RED bold, "You are Successful in creating a user.");
        pal_pause();
        break;
    }
}

int user_exists(void)
{
    return file_exists(USER_FILE);
}