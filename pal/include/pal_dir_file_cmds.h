/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-05
Date Last Modified: 2026-03-05
Module: PAL
File: pal_dir_file_cmds.h
About: PAL abstraction for directory and file operations.
       Provides cross-platform filesystem commands
       used by the OE shell.

       These commands abstract Linux and Windows
       filesystem operations.

Revisions:
- 2026-03-05  Initial creation
------------------------------------------------------------
*/
#ifndef PAL_DIR_FILE_CMDS_H
#define PAL_DIR_FILE_CMDS_H
/* ================= PLATFORM ================= */
#include "pal.h"
#include "ui_setup.h"
#include  "ui_elements.h"
#ifdef _WIN32
    #define OE_FILE_DIR_CMDS_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #define NOMINMAX
    /* Disable deprecation warnings for standard C functions */
    //#define _CRT_SECURE_NO_WARNINGS
    // #define _CRT_NONSTDC_NO_DEPRECATE
#else
    #define OE_FILE_DIR_CMDS_LINUX
#endif
/*
*
* mdr - make directory
* rdr - remove directory
* ldr - list directory contents
* cdr - change directory (forward)
* cdr .. - change directory (backward)
* cpdr - copy directory
* rnmdr - rename directory
* mvdr - move directory
* mdrcd - make directory and change into it
* rmf - remove file
* cpf - copy file
* rdf - read file (display contents)
* rnmf - rename file
* mvf - move file
*
*/

/*
* m - make
* r - remove
* l - list
* c - change
* cp - copy
* rn - rename
* mv - move
* dr - directory
* f - file
*/


/* -------- DIRECTORY COMMANDS -------- */

int pal_mdr(const char* path);
int pal_rdr(const char* path);
int pal_ldr(const char* path);
int pal_cdr(const char* path);
int pal_pwd(char* buffer, int size);
int pal_cpdr(const char* src, const char* dst);
int pal_rnmdr(const char* oldname, const char* newname);
int pal_mvdr(const char* src, const char* dst);


/* -------- FILE COMMANDS -------- */

int pal_rmf(const char* file);
int pal_cpf(const char* src, const char* dst);
int pal_rdf(const char* file);

int pal_rnmf(const char* oldname, const char* newname);
int pal_mvf(const char* src, const char* dst);

#endif