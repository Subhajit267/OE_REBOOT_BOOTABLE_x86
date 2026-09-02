/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-10
Date Last Modified: 2026-03-10
Module: Applications
File: notepad_edit.h
About: Public interface for document editing, file I/O,
       clipboard, and search/replace operations.
Revisions:
- 2026-03-10  Extracted from monolithic notepad.c
------------------------------------------------------------
*/

#ifndef NOTEPAD_EDIT_H
#define NOTEPAD_EDIT_H

#include "notepad_priv.h"

/* Document life cycle */
void np_doc_init(void);
void np_doc_free(void);

/* Editing primitives */
void np_insert(char ch);
void np_newline(void);
void np_backspace(void);
void np_delete(void);
void np_move(int sc);

/* Clipboard */
void np_copy_line(void);
void np_cut_line(void);
void np_paste_line(void);

/* File I/O */
void np_load(const char* fname);
void np_save(const char* fname);

/* Search & replace */
int np_find_str(const char* needle, int sl, int sc, int* fl, int* fc);
void np_replace_one(const char* find, const char* repl);
int np_replace_all(const char* find, const char* repl);

#endif