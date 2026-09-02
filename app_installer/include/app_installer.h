/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: Installer
File: app_installer.h
About: Generic binary-based installer management system
       for OE applications.

Revisions:
- 2026-02-21  Initial implementation
------------------------------------------------------------
*/

#ifndef APP_INSTALLER_H
#define APP_INSTALLER_H
#include"pal.h"

/* -------- Application Structure -------- */

typedef struct
{
    const char* name;      /* Command name of app */
    const char* reg_file;  /* Binary registry file (.rg) */
} app_t;


/* -------- Core Installer Operations -------- */

void install_app(const app_t* app);
void uninstall_app(const app_t* app);

void install_all(void);
void uninstall_all(bool flag);

int app_is_installed(const app_t* app);

/* -------- Lookup -------- */

const app_t* find_app(const char* name);

#endif