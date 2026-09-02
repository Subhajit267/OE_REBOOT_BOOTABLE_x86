/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-10
Date Last Modified: 2026-03-10
Module: Applications
File: notepad_priv.h
About: Private internal header for OE notepad.
       Contains constants, data structures, and extern
       declarations shared among the modular components.
Revisions:
- 2026-03-10  Extracted from monolithic notepad.c
------------------------------------------------------------
*/

#ifndef NOTEPAD_PRIV_H
#define NOTEPAD_PRIV_H

/* ================= WINDOW GEOMETRY ================= */
#define NP_ROW0     2
#define NP_COL0     1
#define NP_WIN_ROWS 23
#define NP_WIN_COLS 78
#define NP_EDIT_ROWS (NP_WIN_ROWS - 2)

/* ================= COLOUR SCHEME ================= */
/* Edit area: black bg, bright-white fg */
#define NP_C_EDIT   "\033[97;44m"

/* Ribbon / status: light grey bg (B7 = \x1B[47m), black fg */
#define NP_C_MENU   "\033[30;47m"

/* Selected menu item: grey bg, black fg */
#define NP_C_SEL    "\033[97;100m"

/* Reset all attributes */
#define NP_C_RESET  "\033[0m"

/* ================= LIMITS ================= */
#define NP_MAX_LINE   4096
#define NP_TAB_STOP      4
#define NP_MENU_COUNT    5
#define NP_MENU_ITEMS   10
#define NP_FNAME_MAX   256
#define NP_CLIP_MAX   4096
#define NP_SRCH_MAX    256

/* ================= DIRTY FLAGS ================= */
#define NP_DIRTY_STATUS  0x01
#define NP_DIRTY_LINE    0x02
#define NP_DIRTY_EDIT    0x04
#define NP_DIRTY_BAR     0x08
#define NP_DIRTY_DROP    0x10
#define NP_DIRTY_ALL     (NP_DIRTY_STATUS|NP_DIRTY_LINE|NP_DIRTY_EDIT|NP_DIRTY_BAR|NP_DIRTY_DROP)

/* ================= DATA TYPES ================= */
typedef struct {
    char  name[12];
    char  items[NP_MENU_ITEMS][32];
    char  keys[NP_MENU_ITEMS];
    int   count;
} NP_Menu;

typedef struct {
    char** lines;
    int    capacity;
    int    count;
    int    cx;        /* cursor col in document (0-based) */
    int    cy;        /* cursor row in document (0-based) */
    int    top;       /* first visible document row       */
    char* filename;
    int    modified;
    int    menu_on;   /* 0=off  1=bar selected  2=dropdown open */
    int    sel_m;     /* selected menu index */
    int    sel_i;     /* selected item index */
} NP_Doc;

/* ================= GLOBAL VARIABLES (defined in notepad_main.c) ================= */
extern NP_Doc   D;
extern NP_Menu  menus[NP_MENU_COUNT];
extern char     last_search[NP_SRCH_MAX];
extern char     np_clip[NP_CLIP_MAX];
extern int      np_running;

#endif