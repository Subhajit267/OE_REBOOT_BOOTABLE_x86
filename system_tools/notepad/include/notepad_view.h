/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-10
Date Last Modified: 2026-03-10
Module: Applications
File: notepad_view.h
About: Public interface for rendering functions and
       user prompts.
Revisions:
- 2026-03-10  Extracted from monolithic notepad.c
- 2026-03-10  Added np_init_background() for initial paint
------------------------------------------------------------
*/

#ifndef NOTEPAD_VIEW_H
#define NOTEPAD_VIEW_H

#include "notepad_priv.h"

/* Full redraw dispatcher */
void np_redraw(int dirty);

/* Viewport adjustment */
void np_scroll(void);

/* Inline prompt in status bar */
void np_prompt(const char* msg, char* out, int maxlen);

/* Flash message in status bar, wait for key */
void np_msg(const char* msg);

/* Paint the entire window with the edit background color */
void np_init_background(void);

#endif