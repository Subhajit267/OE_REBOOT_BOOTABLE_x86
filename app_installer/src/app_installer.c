/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: Installer
File: app_installer.c
About: Generic binary-based installer logic for OE_REBOOT.
       Uses file abstraction and UI layer.
Revisions:
- 2026-02-21  Initial implementation
- 2026-02-21  Removed std string usage
- 2026-02-21  Replaced strcmp with pal_strcmp

ALL CHECKED AND WORKING
------------------------------------------------------------
*/

//#include "pal.h"
#include "app_installer.h"
#include "app_table.h"
#include "file.h"
#include "ui_setup.h"

/* ================= INSTALL CHECK ================= */

int app_is_installed(const app_t* app)
{
    if (app == 0)
        return 0;

    return file_read_int(app->reg_file);
}

/* ================= INSTALL ================= */

void install_app(const app_t* app)
{
    if (app == 0)
        return;

    if (app_is_installed(app))
    {
        ui_status(STATUS_INFO);
        return;
    }

    file_write_int(app->reg_file, 1);
    ui_status(STATUS_SUCCESS);
}

/* ================= UNINSTALL ================= */

void uninstall_app(const app_t* app)
{
    if (app == 0)
        return;

    if (!app_is_installed(app))
    {
        ui_status(STATUS_NOT_INSTALLED);
        return;
    }

    file_write_int(app->reg_file, 0);
    ui_status(STATUS_SUCCESS);
}

/* ================= INSTALL ALL ================= */

void install_all(void)
{
    for (int i = 0; i < app_count; i++)
        file_write_int(apps[i].reg_file, 1);

    ui_status(STATUS_SUCCESS);
}

/* ================= UNINSTALL ALL ================= */

void uninstall_all(bool flag) // flag to supress the messagge when called from main_installer
{
    for (int i = 0; i < app_count; i++)
        file_write_int(apps[i].reg_file, 0);
    if(flag)
        ui_status(STATUS_SUCCESS);
}

/* ================= FIND APP ================= */

const app_t* find_app(const char* name)
{
    if (name == 0)
        return 0;

    for (int i = 0; i < app_count; i++)
    {
        if (pal_strcmp(apps[i].name, name) == 0)
            return &apps[i];
    }

    return 0;
}