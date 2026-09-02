/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: Installer
File: app_table.h
About: Central application registry for OE system.
       Contains all installable applications.

Revisions:
- 2026-02-21  Initial implementation
- 2026-02-21  Expanded to include all OE apps
------------------------------------------------------------
*/

#ifndef APP_TABLE_H
#define APP_TABLE_H

#include "app_installer.h"

/* Global application registry */
extern app_t apps[];
extern int app_count;

#endif