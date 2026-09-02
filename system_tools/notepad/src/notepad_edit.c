/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-10
Date Last Modified: 2026-03-10
Module: Applications
File: notepad_edit.c
About: Implementation of document editing, file I/O,
       clipboard, and search/replace functions.
Revisions:
- 2026-03-10  Extracted from monolithic notepad.c
------------------------------------------------------------
*/

#include "notepad_edit.h"
#include "notepad_priv.h"
#include "pal.h"
#include "ui_setup.h"

/* ================= DOCUMENT INIT / FREE ================= */
void np_doc_free(void)
{
    int i;
    if (D.lines) {
        for (i = 0; i < D.count; i++) pal_free(D.lines[i]);
        pal_free(D.lines);
        D.lines = NULL;
    }
    if (D.filename) { pal_free(D.filename); D.filename = NULL; }
    D.count = 0;
}

void np_doc_init(void)
{
    np_doc_free();
    D.capacity = 128;
    D.lines = (char**)pal_alloc(D.capacity * (int)sizeof(char*));
    D.lines[0] = (char*)pal_alloc(1);
    D.lines[0][0] = '\0';
    D.count = 1;
    D.cx = D.cy = D.top = 0;
    D.modified = 0;
}

/* ================= DOCUMENT EDITING ================= */
void np_insert(char ch)
{
    char* line = D.lines[D.cy];
    int   len = (int)pal_strlen(line);
    line = (char*)pal_realloc(line, len + 2);
    pal_memmove(line + D.cx + 1, line + D.cx, len - D.cx + 1);
    line[D.cx] = ch;
    D.lines[D.cy] = line;
    D.cx++;
    D.modified = 1;
}

void np_newline(void)
{
    char* line = D.lines[D.cy];
    int   len = (int)pal_strlen(line);

    /* New line = text after cursor */
    char* nl = (char*)pal_alloc(len - D.cx + 1);
    pal_strcpy(nl, line + D.cx);

    /* Truncate current line at cursor */
    line[D.cx] = '\0';
    D.lines[D.cy] = (char*)pal_realloc(line, D.cx + 1);

    /* Grow lines array */
    if (D.count >= D.capacity) {
        D.capacity *= 2;
        D.lines = (char**)pal_realloc(D.lines, D.capacity * (int)sizeof(char*));
    }
    int i;
    for (i = D.count; i > D.cy + 1; i--) D.lines[i] = D.lines[i - 1];
    D.lines[D.cy + 1] = nl;
    D.count++;
    D.cy++;
    D.cx = 0;
    D.modified = 1;
}

void np_backspace(void)
{
    if (D.cx > 0) {
        char* line = D.lines[D.cy];
        int   len = (int)pal_strlen(line);
        pal_memmove(line + D.cx - 1, line + D.cx, len - D.cx + 1);
        D.lines[D.cy] = (char*)pal_realloc(line, len);
        D.cx--;
        D.modified = 1;
    }
    else if (D.cy > 0) {
        char* prev = D.lines[D.cy - 1];
        char* curr = D.lines[D.cy];
        int   plen = (int)pal_strlen(prev);
        int   clen = (int)pal_strlen(curr);
        prev = (char*)pal_realloc(prev, plen + clen + 1);
        pal_strcat(prev, curr);
        D.lines[D.cy - 1] = prev;
        pal_free(curr);
        int i;
        for (i = D.cy; i < D.count - 1; i++) D.lines[i] = D.lines[i + 1];
        D.count--;
        D.cy--;
        D.cx = plen;
        D.modified = 1;
    }
}

void np_delete(void)
{
    char* line = D.lines[D.cy];
    int   len = (int)pal_strlen(line);

    if (D.cx < len) {
        pal_memmove(line + D.cx, line + D.cx + 1, len - D.cx);
        D.lines[D.cy] = (char*)pal_realloc(line, len);
        D.modified = 1;
    }
    else if (D.cy < D.count - 1) {
        char* next = D.lines[D.cy + 1];
        int   nlen = (int)pal_strlen(next);
        line = (char*)pal_realloc(line, len + nlen + 1);
        pal_strcat(line, next);
        D.lines[D.cy] = line;
        pal_free(next);
        int i;
        for (i = D.cy + 1; i < D.count - 1; i++) D.lines[i] = D.lines[i + 1];
        D.count--;
        D.modified = 1;
    }
}

void np_move(int sc)
{
    int ll;
    switch (sc) {
    case PAL_SC_LEFT:
        if (D.cx > 0) D.cx--;
        break;
    case PAL_SC_RIGHT:
        if (D.cx < (int)pal_strlen(D.lines[D.cy])) D.cx++;
        break;
    case PAL_SC_UP:
        if (D.cy > 0) {
            D.cy--;
            ll = (int)pal_strlen(D.lines[D.cy]);
            if (D.cx > ll) D.cx = ll;
        }
        break;
    case PAL_SC_DOWN:
        if (D.cy < D.count - 1) {
            D.cy++;
            ll = (int)pal_strlen(D.lines[D.cy]);
            if (D.cx > ll) D.cx = ll;
        }
        break;
    case PAL_SC_HOME:
        D.cx = 0;
        break;
    case PAL_SC_END:
        D.cx = (int)pal_strlen(D.lines[D.cy]);
        break;
    case PAL_SC_PGUP:
        D.cy -= NP_EDIT_ROWS;
        if (D.cy < 0) D.cy = 0;
        ll = (int)pal_strlen(D.lines[D.cy]);
        if (D.cx > ll) D.cx = ll;
        break;
    case PAL_SC_PGDN:
        D.cy += NP_EDIT_ROWS;
        if (D.cy >= D.count) D.cy = D.count - 1;
        ll = (int)pal_strlen(D.lines[D.cy]);
        if (D.cx > ll) D.cx = ll;
        break;
    }
}

/* ================= CLIPBOARD ================= */
void np_copy_line(void)
{
    pal_strncpy(np_clip, D.lines[D.cy], NP_CLIP_MAX);
}

void np_cut_line(void)
{
    np_copy_line();
    if (D.count == 1) {
        pal_free(D.lines[0]);
        D.lines[0] = (char*)pal_alloc(1);
        D.lines[0][0] = '\0';
        D.cx = 0;
    }
    else {
        pal_free(D.lines[D.cy]);
        int i;
        for (i = D.cy; i < D.count - 1; i++) D.lines[i] = D.lines[i + 1];
        D.count--;
        if (D.cy >= D.count) D.cy = D.count - 1;
        int ll = (int)pal_strlen(D.lines[D.cy]);
        if (D.cx > ll) D.cx = ll;
    }
    D.modified = 1;
}

void np_paste_line(void)
{
    int i;
    for (i = 0; np_clip[i]; i++)
        if (np_clip[i] >= 32 && np_clip[i] <= 126)
            np_insert(np_clip[i]);
}

/* ================= FILE I/O ================= */
void np_load(const char* fname)
{
    pal_file_t* fp = pal_file_open_read(fname);
    if (!fp) return;

    int i;
    for (i = 0; i < D.count; i++) pal_free(D.lines[i]);
    D.count = 0;

    char linebuf[NP_MAX_LINE];
    int  col = 0;
    char ch;

    while (pal_file_read(fp, &ch, 1) == 1) {
        if (ch == '\r') continue;
        if (ch == '\n') {
            linebuf[col] = '\0';
            if (D.count >= D.capacity) {
                D.capacity *= 2;
                D.lines = (char**)pal_realloc(D.lines, D.capacity * (int)sizeof(char*));
            }
            D.lines[D.count] = (char*)pal_alloc(col + 1);
            pal_strcpy(D.lines[D.count], linebuf);
            D.count++;
            col = 0;
        }
        else {
            if (col < NP_MAX_LINE - 1) linebuf[col++] = ch;
        }
    }

    /* Last line without trailing newline */
    linebuf[col] = '\0';
    if (D.count >= D.capacity) {
        D.capacity *= 2;
        D.lines = (char**)pal_realloc(D.lines, D.capacity * (int)sizeof(char*));
    }
    D.lines[D.count] = (char*)pal_alloc(col + 1);
    pal_strcpy(D.lines[D.count], linebuf);
    D.count++;

    pal_file_close(fp);

    if (D.count == 0) {
        D.lines[0] = (char*)pal_alloc(1);
        D.lines[0][0] = '\0';
        D.count = 1;
    }

    D.cx = D.cy = D.top = 0;
    if (D.filename) pal_free(D.filename);
    D.filename = pal_strdup(fname);
    D.modified = 0;
}

void np_save(const char* fname)
{
    pal_file_t* fp = pal_file_open_write(fname);
    if (!fp) return;
    int i;
    for (i = 0; i < D.count; i++) {
        pal_file_write(fp, D.lines[i], (int)pal_strlen(D.lines[i]));
        pal_file_write(fp, "\n", 1);
    }
    pal_file_close(fp);
    if (D.filename) pal_free(D.filename);
    D.filename = pal_strdup(fname);
    D.modified = 0;
}

/* ================= SEARCH & REPLACE ================= */
int np_find_str(const char* needle, int sl, int sc, int* fl, int* fc)
{
    int nl = (int)pal_strlen(needle);
    if (nl == 0) return 0;
    int l;
    for (l = sl; l < D.count; l++) {
        char* line = D.lines[l];
        int   ll = (int)pal_strlen(line);
        int   s = (l == sl) ? sc : 0;
        int   c;
        for (c = s; c <= ll - nl; c++) {
            if (pal_strnicmp(line + c, needle, nl) == 0) {
                *fl = l; *fc = c; return 1;
            }
        }
    }
    return 0;
}

void np_replace_one(const char* find, const char* repl)
{
    int l = -1, c = 0;
    if (!np_find_str(find, D.cy, D.cx, &l, &c))
        np_find_str(find, 0, 0, &l, &c);
    if (l < 0) return;
    D.cy = l; D.cx = c;
    int flen = (int)pal_strlen(find), i;
    for (i = 0; i < flen; i++) np_delete();
    for (i = 0; repl[i]; i++)  np_insert(repl[i]);
}

int np_replace_all(const char* find, const char* repl)
{
    int count = 0, l, c, flen = (int)pal_strlen(find);
    int sy = 0, sx = 0;
    while (np_find_str(find, sy, sx, &l, &c)) {
        D.cy = l; D.cx = c;
        int i;
        for (i = 0; i < flen; i++) np_delete();
        for (i = 0; repl[i]; i++) np_insert(repl[i]);
        count++;
        sy = D.cy; sx = D.cx;
    }
    return count;
}