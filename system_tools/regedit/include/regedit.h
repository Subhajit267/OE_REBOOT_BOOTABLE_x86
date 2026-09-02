/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: System Tools
File: regedit.h
About: Registry Editor interface header.
       Provides entry point for Registry Editor module.
Revisions:
- 2026-02-21  Initial definition
- 2026-02-21  Converted to structured menu-based system
------------------------------------------------------------
*/

#ifndef REGEDIT_H
#define REGEDIT_H

/* ================= GLOBAL STATE ================= */

/*
Flag indicating whether admin mode is active.

0 → Normal user
1 → Admin mode
*/
extern int reg_admin_mode;

/* ================= CORE FUNCTION ================= */

/*
Runs the Registry Editor main loop.

This function:
- Displays structured menu using ui_menu()
- Handles install/reset/temp operations
- Allows admin mode activation
- Returns to caller on exit
*/
void reg_edit(void);

#endif