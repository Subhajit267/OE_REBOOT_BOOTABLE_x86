/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-10
Date Last Modified: 2026-03-10
Module: Applications
File: notepad_view.c
About: Implementation of all rendering functions,
       including the menu bar, dropdowns, edit area,
       status bar, and viewport management.
Revisions:
- 2026-03-10  Extracted from monolithic notepad.c
- 2026-03-10  Added np_init_background() for initial paint
- 2026-08-26  BUG FIX: np_draw_status()'s trailing shortcut hint
              alone was 87 visible chars — combined with the
              filename/modified-marker/Ln/Col prefix this overflowed
              the 78-col window on every single draw (~110 chars
              minimum). Shortened the hint to "F10=Menu for
              shortcuts" and capped the displayed filename to 23
              chars so the worst case still fits.
- 2026-08-26  BUG FIX: np_prompt() echoed every typed character with
              no check against the window's right edge — typing past
              roughly col 66-70 (label-length dependent) ran off the
              78-col window and wrapped/corrupted the fixed grid.
              Echo (and its backspace erase) now stop once the
              cursor would leave the window; the full string is
              still recorded into "out" regardless.
------------------------------------------------------------
*/

#include "notepad_view.h"
#include "notepad_priv.h"
#include "pal.h"
#include "ui_setup.h"

/* ================= OUTPUT HELPERS ================= */
static void np_goto(int wr, int wc)
{
    pal_set_cursor(NP_ROW0 + wr, NP_COL0 + wc);
}

static void np_fill(int n)
{
    int i;
    for (i = 0; i < n; i++) pal_putchar(' ');
}

/* ================= INITIAL BACKGROUND ================= */
void np_init_background(void)
{
    static char blank[32 + NP_WIN_COLS + 4];
    int clen = (int)pal_strlen(NP_C_EDIT);
    pal_memcpy(blank, NP_C_EDIT, clen);
    pal_memset(blank + clen, ' ', NP_WIN_COLS);
    blank[clen + NP_WIN_COLS] = '\0';
    int row;
    for (row = 0; row < NP_WIN_ROWS; row++) {
        np_goto(row, 0);
        pal_print(blank);
    }
    pal_print(NP_C_RESET);
}

/* ================= RENDERING ================= */
static void np_draw_bar(void)
{
    static char bar_buf[32 + NP_WIN_COLS + 8];
    int pos = 0;
    int i;

    pal_memcpy(bar_buf + pos, NP_C_MENU, (int)pal_strlen(NP_C_MENU));
    pos += (int)pal_strlen(NP_C_MENU);

    pal_memset(bar_buf + pos, ' ', NP_WIN_COLS);

    int x = 2;
    for (i = 0; i < NP_MENU_COUNT; i++) {
        int sel = (D.menu_on > 0) && (i == D.sel_m);
        int nlen = (int)pal_strlen(menus[i].name);
        (void)sel; /* not used in this simplified pass */
        x += nlen + 3;
    }

    bar_buf[pos + NP_WIN_COLS] = '\0';
    np_goto(0, 0);
    pal_print(bar_buf);

    x = 2;
    for (i = 0; i < NP_MENU_COUNT; i++) {
        int sel = (D.menu_on > 0) && (i == D.sel_m);
        int nlen = (int)pal_strlen(menus[i].name);
        np_goto(0, x);
        pal_print(sel ? NP_C_SEL : NP_C_MENU);
        pal_putchar(' ');
        pal_print(menus[i].name);
        pal_putchar(' ');
        x += nlen + 3;
    }
    pal_print(NP_C_RESET);
}

static void np_draw_dropdown(int idx)
{
    if (idx < 0 || idx >= NP_MENU_COUNT) return;

    int sx = 2, i;
    for (i = 0; i < idx; i++) sx += (int)pal_strlen(menus[i].name) + 3;

    int w = 0;
    for (i = 0; i < menus[idx].count; i++) {
        int l = (int)pal_strlen(menus[idx].items[i]);
        if (l > w) w = l;
    }
    w += 4;

    pal_print(NP_C_MENU);
    int y;
    for (y = 1; y <= menus[idx].count + 2; y++) {
        int x;
        for (x = sx; x < sx + w; x++) {
            np_goto(y, x);
            if (y == 1 || y == menus[idx].count + 2)
                pal_putchar(x == sx || x == sx + w - 1 ? '+' : '-');
            else
                pal_putchar(x == sx || x == sx + w - 1 ? '|' : ' ');
        }
    }

    for (i = 0; i < menus[idx].count; i++) {
        int sel = (D.menu_on == 2) && (i == D.sel_i);
        np_goto(2 + i, sx + 2);
        pal_print(sel ? NP_C_SEL : NP_C_MENU);
        pal_print(menus[idx].items[i]);
        int pad = w - 4 - (int)pal_strlen(menus[idx].items[i]);
        if (pad > 0) np_fill(pad);
    }
    pal_print(NP_C_RESET);
}

static void np_draw_status(void)
{
    static char st_buf[32 + NP_WIN_COLS + 8];
    int pos = 0;

    pal_memcpy(st_buf + pos, NP_C_MENU, (int)pal_strlen(NP_C_MENU));
    pos += (int)pal_strlen(NP_C_MENU);

    pal_memset(st_buf + pos, ' ', NP_WIN_COLS);
    st_buf[pos + NP_WIN_COLS] = '\0';
    np_goto(NP_WIN_ROWS - 1, 0);
    pal_print(st_buf);

    char fname[24];
    char lnbuf[10], colbuf[10];
    if (D.filename) pal_strncpy(fname, D.filename, 24);
    else            pal_strcpy(fname, "[Untitled]");
    pal_itoa(D.cy + 1, lnbuf);
    pal_itoa(D.cx + 1, colbuf);

    np_goto(NP_WIN_ROWS - 1, 1);
    pal_print(NP_C_MENU);
    pal_print(fname);
    if (D.modified) pal_print(" *");
    pal_print("  Ln ");  pal_print(lnbuf);
    pal_print("  Col "); pal_print(colbuf);
    pal_print("  |  F10=Menu for shortcuts");
    pal_print(NP_C_RESET);
}

static void np_draw_one_line(int dy)
{
    int safe = NP_WIN_COLS - 1;
    int wr = 1 + (dy - D.top);
    if (wr < 1 || wr > NP_EDIT_ROWS) return;

    static char lb[32 + NP_WIN_COLS + 4];
    int clen = (int)pal_strlen(NP_C_EDIT);
    pal_memcpy(lb, NP_C_EDIT, clen);
    char* p = lb + clen;

    if (dy < D.count) {
        char* line = D.lines[dy];
        int   len = (int)pal_strlen(line);
        if (len > safe) len = safe;
        pal_memcpy(p, line, len);
        int i;
        for (i = 0; i < len; i++)
            if ((unsigned char)p[i] > 126 || (unsigned char)p[i] < 32) p[i] = '.';
        pal_memset(p + len, ' ', safe - len);
    }
    else {
        p[0] = '~';
        pal_memset(p + 1, ' ', safe - 1);
    }
    p[safe] = '\0';

    np_goto(wr, 0);
    pal_print(lb);
    pal_print(NP_C_RESET);
}

static void np_draw_edit(void)
{
    static char row_buf[32 + NP_WIN_COLS + 8];
    int  safe = NP_WIN_COLS - 1;
    int  clen = (int)pal_strlen(NP_C_EDIT);
    int  row;

    pal_memcpy(row_buf, NP_C_EDIT, clen);
    pal_hide_cursor();
    np_goto(1, 0);

    for (row = 0; row < NP_EDIT_ROWS; row++) {
        int  dr = row + D.top;
        char* p = row_buf + clen;

        if (dr < D.count) {
            char* line = D.lines[dr];
            int   len = (int)pal_strlen(line);
            if (len > safe) len = safe;
            pal_memcpy(p, line, len);
            int i;
            for (i = 0; i < len; i++)
                if ((unsigned char)p[i] > 126 || (unsigned char)p[i] < 32)
                    p[i] = '.';
            pal_memset(p + len, ' ', safe - len);
        }
        else {
            p[0] = '~';
            pal_memset(p + 1, ' ', safe - 1);
        }

        p[safe] = '\r';
        p[safe + 1] = '\n';
        p[safe + 2] = '\0';

        pal_print(row_buf);

        if (row == 0) {
            pal_memmove(row_buf, row_buf + clen, safe + 3);
            clen = 0;
        }
    }

    pal_print(NP_C_RESET);
}

/* -------- Full redraw dispatcher -------- */
void np_redraw(int dirty)
{
    pal_hide_cursor();

    if (dirty & NP_DIRTY_EDIT)   np_draw_edit();
    else if (dirty & NP_DIRTY_LINE) np_draw_one_line(D.cy);

    if (dirty & NP_DIRTY_BAR)    np_draw_bar();
    if (dirty & NP_DIRTY_STATUS) np_draw_status();
    if (dirty & NP_DIRTY_DROP) { if (D.menu_on == 2) np_draw_dropdown(D.sel_m); }

    int scr_row = 1 + (D.cy - D.top);
    int scr_col = D.cx;
    if (scr_col >= NP_WIN_COLS - 1) scr_col = NP_WIN_COLS - 2;
    np_goto(scr_row, scr_col);
    pal_show_cursor();
}

/* -------- Scroll viewport to keep cursor visible -------- */
void np_scroll(void)
{
    if (D.cy < D.top)
        D.top = D.cy;
    else if (D.cy >= D.top + NP_EDIT_ROWS)
        D.top = D.cy - NP_EDIT_ROWS + 1;
}

/* ================= INLINE PROMPT ================= */
void np_prompt(const char* msg, char* out, int maxlen)
{
    pal_print(NP_C_MENU);
    np_goto(NP_WIN_ROWS - 1, 0);
    np_fill(NP_WIN_COLS);
    np_goto(NP_WIN_ROWS - 1, 1);
    pal_print(msg);
    pal_print(NP_C_RESET);
    pal_show_cursor();

    int pos = 0;
    int plen = (int)pal_strlen(msg);
    out[0] = '\0';

    while (1) {
        int k = pal_raw_getkey();
        if (k == 0)              continue;
        if (k == '\r' || k == '\n') break;
        if (k == '\033') { out[0] = '\0'; break; }
        if (PAL_KEY_IS_BACKSPACE(k)) {
            if (pos > 0) {
                pos--;
                out[pos] = '\0';
                if (1 + plen + pos < NP_WIN_COLS) {
                    np_goto(NP_WIN_ROWS - 1, 1 + plen + pos);
                    pal_putchar(' ');
                    np_goto(NP_WIN_ROWS - 1, 1 + plen + pos);
                }
            }
        }
        else if (!PAL_KEY_IS_EXT(k) && k >= 32 && k <= 126 && pos < maxlen - 1) {
            out[pos++] = (char)k;
            out[pos] = '\0';
            /* Keep recording into "out" past the window edge, but stop
               echoing so the terminal doesn't auto-wrap and corrupt
               the fixed 78-col layout. */
            if (1 + plen + pos <= NP_WIN_COLS)
                pal_putchar((char)k);
        }
    }
    pal_hide_cursor();
}

void np_msg(const char* msg)
{
    pal_print(NP_C_MENU);
    np_goto(NP_WIN_ROWS - 1, 0);
    np_fill(NP_WIN_COLS);
    np_goto(NP_WIN_ROWS - 1, 1);
    pal_print(msg);
    pal_print(NP_C_RESET);
    int k;
    do { k = pal_raw_getkey(); } while (k == 0);
}