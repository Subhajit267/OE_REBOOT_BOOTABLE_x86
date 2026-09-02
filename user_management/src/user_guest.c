/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-22
Date Last Modified: 2026-02-22
Module: User Management
File: user_guest.c
About: Guest mode entry.
Revisions:
- 2026-02-22  Initial implementation
- 2026-03-16  reg_status renamed to is_guest_mode
------------------------------------------------------------
*/

#include "pal.h"
#include "user.h"
#include "prompt.h"

void user_enter_guest(void)
{
    is_guest_mode = 1;
    prompt();
}