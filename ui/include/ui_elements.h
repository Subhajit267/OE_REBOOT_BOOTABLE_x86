/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: UI Elements
File: ui_elements.h
About: Drawing primitives and theme engine for OE_REBOOT
Revisions:
- 2026-02-21  Initial implementation
- 2026-02-21  Integrated border theme loader
------------------------------------------------------------
*/

#ifndef UI_ELEMENTS_H
#define UI_ELEMENTS_H
#include "ui_coordinates.h"
/* ================= THEME ================= */

/* Returns border color (1-15) from looks.bd */
int ui_get_border_color(void);

/* Sets border color (writes to looks.bd) */
//void ui_set_border_color(int color);

/* ================= DRAWING ================= */

void layout(void);
//void border(void);

//void hline(int row, int col, int length);
//void vline(int row, int col, int height);
//void box(int row, int col, int width, int height);

void logo(void);

/*
Progress bar
row, col = starting position
width = number of blocks
*/
void progressbar(int row, int col, int width);

#endif