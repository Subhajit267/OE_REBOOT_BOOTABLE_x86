///*
//------------------------------------------------------------
//Author: Subhajit Halder
//Date Created: 2026-03-10
//Date Last Modified: 2026-03-10
//Module: Applications
//File: notepad.c
//About: OE text editor – PAL-native.
//
//       Only headers used: pal.h, ui_setup.h.
//       No std C headers, no platform headers.
//
//       Window geometry:
//         Fills the OE interior exactly – the notepad sits
//         inside the OE border frame without overlapping it.
//           Start row : 2   (terminal row, 1-based)
//           Start col : 4   (terminal col, 1-based)
//           Width     : 203 columns
//           Height    : 48  rows
//         Row 1 and row 50 are the OE border bars.
//         Cols 3 and 207 are the OE left/right border cells.
//
//       Colour scheme (three colours only):
//         NP_C_EDIT   – blue background, bright-white text
//                       used for the edit area
//         NP_C_MENU   – grey background (B7), black text
//                       used for ribbon and status bar,
//                       and for unselected menu item text
//         NP_C_SEL    – grey background (B7), bright-white text
//                       used for the currently highlighted
//                       menu item so it stands out clearly
//
//       Menu ribbon:
//         F10 opens/closes the top ribbon.
//         File  Edit  Search  Options  Help
//         Arrow keys navigate, Enter selects, ESC closes.
//         Hotkeys work inside open dropdowns.
//
//       Alt shortcuts (no OS shortcut conflicts):
//         Alt+S  Save          Alt+N  New
//         Alt+F  Find          Alt+R  Replace
//         Alt+X  Cut line      Alt+C  Copy line
//         Alt+V  Paste
//
//Revisions:
//- 2026-03-10  Initial implementation
//------------------------------------------------------------
//*/
//
//#include "pal.h"
//#include "ui_setup.h"
//#include "notepad.h"
//#include "ui_elements.h"
///* ================= WINDOW GEOMETRY ================= */
///*
//pal_set_cursor is 1-based.
//np_goto(wr, wc) maps window-relative 0-based coords to
//terminal 1-based coords:  row = NP_ROW0 + wr,  col = NP_COL0 + wc
//*/
//#define NP_ROW0     2     /* 1-based terminal row of window top    */
//#define NP_COL0     1     /* 1-based terminal col of window left   */
//#define NP_WIN_ROWS 50     /* total rows in window                  */
//#define NP_WIN_COLS 209    /* total cols in window                  */
//
///* Row 0 = ribbon bar
//   Rows 1 .. NP_WIN_ROWS-2 = edit area
//   Row NP_WIN_ROWS-1 = status bar                                   */
//#define NP_EDIT_ROWS  (NP_WIN_ROWS - 2)   /* 46 edit rows */
//
//   /* ================= COLOUR SCHEME ================= */
//   /*
//   Three colours:
//     blue  – edit area background
//     Grey   – ribbon / status bar background + black text (normal items)
//     White  – highlighted text colour inside grey (selected items)
//   */
//
//   /* Edit area: black bg, bright-white fg */
//#define NP_C_EDIT   "\033[97;44m"
//
///* Ribbon / status: light grey bg (B7 = \x1B[47m), black fg */
//#define NP_C_MENU   "\033[30;47m"
//
///* Selected menu item: grey bg, black fg */
//#define NP_C_SEL    "\033[97;100m"
//
///* Reset all attributes */
//#define NP_C_RESET  "\033[0m"
//
///* ================= LIMITS ================= */
//#define NP_MAX_LINE   4096
//#define NP_TAB_STOP      4
//#define NP_MENU_COUNT    5
//#define NP_MENU_ITEMS   10
//#define NP_FNAME_MAX   256
//#define NP_CLIP_MAX   4096
//#define NP_SRCH_MAX    256
//
///* ================= DATA TYPES ================= */
//
//typedef struct {
//    char  name[12];
//    char  items[NP_MENU_ITEMS][32];
//    char  keys[NP_MENU_ITEMS];
//    int   count;
//} NP_Menu;
//
//typedef struct {
//    char** lines;
//    int    capacity;
//    int    count;
//    int    cx;        /* cursor col in document (0-based) */
//    int    cy;        /* cursor row in document (0-based) */
//    int    top;       /* first visible document row       */
//    char* filename;
//    int    modified;
//    int    menu_on;   /* 0=off  1=bar selected  2=dropdown open */
//    int    sel_m;     /* selected menu index */
//    int    sel_i;     /* selected item index */
//} NP_Doc;
//
///* ================= GLOBALS ================= */
//
//static NP_Doc   D;
//static NP_Menu  menus[NP_MENU_COUNT];
//static char     last_search[NP_SRCH_MAX];
//static char     np_clip[NP_CLIP_MAX];
//static int      np_running;
//
///* ================= OUTPUT HELPERS ================= */
//
///* np_goto: position cursor at window-relative 0-based (wr, wc) */
//static void np_goto(int wr, int wc)
//{
//    pal_set_cursor(NP_ROW0 + wr, NP_COL0 + wc);
//}
//
///* Fill n spaces at current position */
//static void np_fill(int n)
//{
//    int i;
//    for (i = 0; i < n; i++) pal_putchar(' ');
//}
//
///* ================= DOCUMENT INIT / FREE ================= */
//
//static void np_doc_free(void)
//{
//    int i;
//    if (D.lines) {
//        for (i = 0; i < D.count; i++) pal_free(D.lines[i]);
//        pal_free(D.lines);
//        D.lines = NULL;
//    }
//    if (D.filename) { pal_free(D.filename); D.filename = NULL; }
//    D.count = 0;
//}
//
//static void np_doc_init(void)
//{
//    np_doc_free();
//    D.capacity = 128;
//    D.lines = (char**)pal_alloc(D.capacity * (int)sizeof(char*));
//    D.lines[0] = (char*)pal_alloc(1);
//    D.lines[0][0] = '\0';
//    D.count = 1;
//    D.cx = D.cy = D.top = 0;
//    D.modified = 0;
//}
//
///* ================= DOCUMENT EDITING ================= */
//
///* -------- Insert char at (cy, cx) -------- */
//static void np_insert(char ch)
//{
//    char* line = D.lines[D.cy];
//    int   len = (int)pal_strlen(line);
//    line = (char*)pal_realloc(line, len + 2);
//    pal_memmove(line + D.cx + 1, line + D.cx, len - D.cx + 1);
//    line[D.cx] = ch;
//    D.lines[D.cy] = line;
//    D.cx++;
//    D.modified = 1;
//}
//
///* -------- Split line at cursor (Enter) -------- */
//static void np_newline(void)
//{
//    char* line = D.lines[D.cy];
//    int   len = (int)pal_strlen(line);
//
//    /* New line = text after cursor */
//    char* nl = (char*)pal_alloc(len - D.cx + 1);
//    pal_strcpy(nl, line + D.cx);
//
//    /* Truncate current line at cursor */
//    line[D.cx] = '\0';
//    D.lines[D.cy] = (char*)pal_realloc(line, D.cx + 1);
//
//    /* Grow lines array */
//    if (D.count >= D.capacity) {
//        D.capacity *= 2;
//        D.lines = (char**)pal_realloc(D.lines, D.capacity * (int)sizeof(char*));
//    }
//    int i;
//    for (i = D.count; i > D.cy + 1; i--) D.lines[i] = D.lines[i - 1];
//    D.lines[D.cy + 1] = nl;
//    D.count++;
//    D.cy++;
//    D.cx = 0;
//    D.modified = 1;
//}
//
///* -------- Backspace -------- */
//static void np_backspace(void)
//{
//    if (D.cx > 0) {
//        char* line = D.lines[D.cy];
//        int   len = (int)pal_strlen(line);
//        pal_memmove(line + D.cx - 1, line + D.cx, len - D.cx + 1);
//        D.lines[D.cy] = (char*)pal_realloc(line, len);
//        D.cx--;
//        D.modified = 1;
//    }
//    else if (D.cy > 0) {
//        char* prev = D.lines[D.cy - 1];
//        char* curr = D.lines[D.cy];
//        int   plen = (int)pal_strlen(prev);
//        int   clen = (int)pal_strlen(curr);
//        prev = (char*)pal_realloc(prev, plen + clen + 1);
//        pal_strcat(prev, curr);
//        D.lines[D.cy - 1] = prev;
//        pal_free(curr);
//        int i;
//        for (i = D.cy; i < D.count - 1; i++) D.lines[i] = D.lines[i + 1];
//        D.count--;
//        D.cy--;
//        D.cx = plen;
//        D.modified = 1;
//    }
//}
//
///* -------- Delete char at cursor -------- */
//static void np_delete(void)
//{
//    char* line = D.lines[D.cy];
//    int   len = (int)pal_strlen(line);
//
//    if (D.cx < len) {
//        pal_memmove(line + D.cx, line + D.cx + 1, len - D.cx);
//        D.lines[D.cy] = (char*)pal_realloc(line, len);
//        D.modified = 1;
//    }
//    else if (D.cy < D.count - 1) {
//        char* next = D.lines[D.cy + 1];
//        int   nlen = (int)pal_strlen(next);
//        line = (char*)pal_realloc(line, len + nlen + 1);
//        pal_strcat(line, next);
//        D.lines[D.cy] = line;
//        pal_free(next);
//        int i;
//        for (i = D.cy + 1; i < D.count - 1; i++) D.lines[i] = D.lines[i + 1];
//        D.count--;
//        D.modified = 1;
//    }
//}
//
///* -------- Cursor movement -------- */
//static void np_move(int sc)
//{
//    int ll;
//    switch (sc) {
//    case PAL_SC_LEFT:
//        if (D.cx > 0) D.cx--;
//        break;
//    case PAL_SC_RIGHT:
//        if (D.cx < (int)pal_strlen(D.lines[D.cy])) D.cx++;
//        break;
//    case PAL_SC_UP:
//        if (D.cy > 0) {
//            D.cy--;
//            ll = (int)pal_strlen(D.lines[D.cy]);
//            if (D.cx > ll) D.cx = ll;
//        }
//        break;
//    case PAL_SC_DOWN:
//        if (D.cy < D.count - 1) {
//            D.cy++;
//            ll = (int)pal_strlen(D.lines[D.cy]);
//            if (D.cx > ll) D.cx = ll;
//        }
//        break;
//    case PAL_SC_HOME:
//        D.cx = 0;
//        break;
//    case PAL_SC_END:
//        D.cx = (int)pal_strlen(D.lines[D.cy]);
//        break;
//    case PAL_SC_PGUP:
//        D.cy -= NP_EDIT_ROWS;
//        if (D.cy < 0) D.cy = 0;
//        ll = (int)pal_strlen(D.lines[D.cy]);
//        if (D.cx > ll) D.cx = ll;
//        break;
//    case PAL_SC_PGDN:
//        D.cy += NP_EDIT_ROWS;
//        if (D.cy >= D.count) D.cy = D.count - 1;
//        ll = (int)pal_strlen(D.lines[D.cy]);
//        if (D.cx > ll) D.cx = ll;
//        break;
//    }
//}
//
///* ================= CLIPBOARD ================= */
//
//static void np_copy_line(void)
//{
//    pal_strncpy(np_clip, D.lines[D.cy], NP_CLIP_MAX);
//}
//
//static void np_cut_line(void)
//{
//    np_copy_line();
//    if (D.count == 1) {
//        pal_free(D.lines[0]);
//        D.lines[0] = (char*)pal_alloc(1);
//        D.lines[0][0] = '\0';
//        D.cx = 0;
//    }
//    else {
//        pal_free(D.lines[D.cy]);
//        int i;
//        for (i = D.cy; i < D.count - 1; i++) D.lines[i] = D.lines[i + 1];
//        D.count--;
//        if (D.cy >= D.count) D.cy = D.count - 1;
//        int ll = (int)pal_strlen(D.lines[D.cy]);
//        if (D.cx > ll) D.cx = ll;
//    }
//    D.modified = 1;
//}
//
//static void np_paste_line(void)
//{
//    int i;
//    for (i = 0; np_clip[i]; i++)
//        if (np_clip[i] >= 32 && np_clip[i] <= 126)
//            np_insert(np_clip[i]);
//}
//
///* ================= FILE I/O ================= */
//
//static void np_load(const char* fname)
//{
//    pal_file_t* fp = pal_file_open_read(fname);
//    if (!fp) return;
//
//    int i;
//    for (i = 0; i < D.count; i++) pal_free(D.lines[i]);
//    D.count = 0;
//
//    char linebuf[NP_MAX_LINE];
//    int  col = 0;
//    char ch;
//
//    while (pal_file_read(fp, &ch, 1) == 1) {
//        if (ch == '\r') continue;
//        if (ch == '\n') {
//            linebuf[col] = '\0';
//            if (D.count >= D.capacity) {
//                D.capacity *= 2;
//                D.lines = (char**)pal_realloc(D.lines, D.capacity * (int)sizeof(char*));
//            }
//            D.lines[D.count] = (char*)pal_alloc(col + 1);
//            pal_strcpy(D.lines[D.count], linebuf);
//            D.count++;
//            col = 0;
//        }
//        else {
//            if (col < NP_MAX_LINE - 1) linebuf[col++] = ch;
//        }
//    }
//
//    /* Last line without trailing newline */
//    linebuf[col] = '\0';
//    if (D.count >= D.capacity) {
//        D.capacity *= 2;
//        D.lines = (char**)pal_realloc(D.lines, D.capacity * (int)sizeof(char*));
//    }
//    D.lines[D.count] = (char*)pal_alloc(col + 1);
//    pal_strcpy(D.lines[D.count], linebuf);
//    D.count++;
//
//    pal_file_close(fp);
//
//    if (D.count == 0) {
//        D.lines[0] = (char*)pal_alloc(1);
//        D.lines[0][0] = '\0';
//        D.count = 1;
//    }
//
//    D.cx = D.cy = D.top = 0;
//    if (D.filename) pal_free(D.filename);
//    D.filename = pal_strdup(fname);
//    D.modified = 0;
//}
//
//static void np_save(const char* fname)
//{
//    pal_file_t* fp = pal_file_open_write(fname);
//    if (!fp) return;
//    int i;
//    for (i = 0; i < D.count; i++) {
//        pal_file_write(fp, D.lines[i], (int)pal_strlen(D.lines[i]));
//        pal_file_write(fp, "\n", 1);
//    }
//    pal_file_close(fp);
//    if (D.filename) pal_free(D.filename);
//    D.filename = pal_strdup(fname);
//    D.modified = 0;
//}
//
///* ================= SEARCH & REPLACE ================= */
//
//static int np_find_str(const char* needle, int sl, int sc, int* fl, int* fc)
//{
//    int nl = (int)pal_strlen(needle);
//    if (nl == 0) return 0;
//    int l;
//    for (l = sl; l < D.count; l++) {
//        char* line = D.lines[l];
//        int   ll = (int)pal_strlen(line);
//        int   s = (l == sl) ? sc : 0;
//        int   c;
//        for (c = s; c <= ll - nl; c++) {
//            if (pal_strnicmp(line + c, needle, nl) == 0) {
//                *fl = l; *fc = c; return 1;
//            }
//        }
//    }
//    return 0;
//}
//
//static void np_replace_one(const char* find, const char* repl)
//{
//    int l = -1, c = 0;
//    if (!np_find_str(find, D.cy, D.cx, &l, &c))
//        np_find_str(find, 0, 0, &l, &c);
//    if (l < 0) return;
//    D.cy = l; D.cx = c;
//    int flen = (int)pal_strlen(find), i;
//    for (i = 0; i < flen; i++) np_delete();
//    for (i = 0; repl[i]; i++)  np_insert(repl[i]);
//}
//
//static int np_replace_all(const char* find, const char* repl)
//{
//    int count = 0, l, c, flen = (int)pal_strlen(find);
//    int sy = 0, sx = 0;
//    while (np_find_str(find, sy, sx, &l, &c)) {
//        D.cy = l; D.cx = c;
//        int i;
//        for (i = 0; i < flen; i++) np_delete();
//        for (i = 0; repl[i]; i++) np_insert(repl[i]);
//        count++;
//        sy = D.cy; sx = D.cx;
//    }
//    return count;
//}
//
///* ================= RENDERING ================= */
//
///* -------- Ribbon bar (window row 0) -------- */
//static void np_draw_bar(void)
//{
//    /* Build entire bar row in one buffer, emit in one call */
//    static char bar_buf[32 + NP_WIN_COLS + 8];
//    int pos = 0;
//    int i;
//
//    /* Colour prefix */
//    pal_memcpy(bar_buf + pos, NP_C_MENU, (int)pal_strlen(NP_C_MENU));
//    pos += (int)pal_strlen(NP_C_MENU);
//
//    /* Fill row with spaces first */
//    pal_memset(bar_buf + pos, ' ', NP_WIN_COLS);
//
//    /* Stamp menu titles over the spaces */
//    int x = 2;
//    for (i = 0; i < NP_MENU_COUNT; i++) {
//        int sel = (D.menu_on > 0) && (i == D.sel_m);
//        int nlen = (int)pal_strlen(menus[i].name);
//        const char* col = sel ? NP_C_SEL : "";
//        const char* rst = sel ? NP_C_MENU : "";
//
//        /* We can't stamp ANSI codes in-buffer at an offset easily,
//           so for the bar we emit in segments when a selected item
//           changes colour; otherwise pure spaces are already set. */
//        (void)col; (void)rst;
//        /* mark positions – actual colour toggling done below */
//        x += nlen + 3;
//    }
//
//    /* Emit the grey background fill */
//    bar_buf[pos + NP_WIN_COLS] = '\0';
//    np_goto(0, 0);
//    pal_print(bar_buf);
//
//    /* Now overdraw each menu title (colour codes prevent doing this
//       purely in a flat buffer) */
//    x = 2;
//    for (i = 0; i < NP_MENU_COUNT; i++) {
//        int sel = (D.menu_on > 0) && (i == D.sel_m);
//        int nlen = (int)pal_strlen(menus[i].name);
//        np_goto(0, x);
//        pal_print(sel ? NP_C_SEL : NP_C_MENU);
//        pal_putchar(' ');
//        pal_print(menus[i].name);
//        pal_putchar(' ');
//        x += nlen + 3;
//    }
//    pal_print(NP_C_RESET);
//}
//
///* -------- Dropdown (window rows 1+) -------- */
//static void np_draw_dropdown(int idx)
//{
//    if (idx < 0 || idx >= NP_MENU_COUNT) return;
//
//    /* X position where this menu title starts in the bar */
//    int sx = 2, i;
//    for (i = 0; i < idx; i++) sx += (int)pal_strlen(menus[i].name) + 3;
//
//    /* Box width = longest item label + 4 (2 padding each side) */
//    int w = 0;
//    for (i = 0; i < menus[idx].count; i++) {
//        int l = (int)pal_strlen(menus[idx].items[i]);
//        if (l > w) w = l;
//    }
//    w += 4;
//
//    /* Draw box outline – grey bg, black text */
//    pal_print(NP_C_MENU);
//    int y;
//    for (y = 1; y <= menus[idx].count + 2; y++) {
//        int x;
//        for (x = sx; x < sx + w; x++) {
//            np_goto(y, x);
//            if (y == 1 || y == menus[idx].count + 2)
//                pal_putchar(x == sx || x == sx + w - 1 ? '+' : '-');
//            else
//                pal_putchar(x == sx || x == sx + w - 1 ? '|' : ' ');
//        }
//    }
//
//    /* Draw items:
//       normal item  = grey bg, black text  (NP_C_MENU)
//       selected item = grey bg, white text (NP_C_SEL)   */
//    for (i = 0; i < menus[idx].count; i++) {
//        int sel = (D.menu_on == 2) && (i == D.sel_i);
//        np_goto(2 + i, sx + 2);
//        pal_print(sel ? NP_C_SEL : NP_C_MENU);
//        pal_print(menus[idx].items[i]);
//        /* Pad to uniform width so the box looks clean */
//        int pad = w - 4 - (int)pal_strlen(menus[idx].items[i]);
//        if (pad > 0) np_fill(pad);
//    }
//    pal_print(NP_C_RESET);
//}
//
///* -------- Status bar (last window row) -------- */
//static void np_draw_status(void)
//{
//    static char st_buf[32 + NP_WIN_COLS + 8];
//    int pos = 0;
//
//    /* Colour prefix */
//    pal_memcpy(st_buf + pos, NP_C_MENU, (int)pal_strlen(NP_C_MENU));
//    pos += (int)pal_strlen(NP_C_MENU);
//
//    /* Fill row with spaces */
//    pal_memset(st_buf + pos, ' ', NP_WIN_COLS);
//    st_buf[pos + NP_WIN_COLS] = '\0';
//    np_goto(NP_WIN_ROWS - 1, 0);
//    pal_print(st_buf);
//
//    /* Overdraw content */
//    char fname[44];
//    char lnbuf[10], colbuf[10];
//    if (D.filename) pal_strncpy(fname, D.filename, 44);
//    else            pal_strcpy(fname, "[Untitled]");
//    pal_itoa(D.cy + 1, lnbuf);
//    pal_itoa(D.cx + 1, colbuf);
//
//    np_goto(NP_WIN_ROWS - 1, 1);
//    pal_print(NP_C_MENU);
//    pal_print(fname);
//    if (D.modified) pal_print(" *");
//    pal_print("  Ln ");  pal_print(lnbuf);
//    pal_print("  Col "); pal_print(colbuf);
//    pal_print("  |  F10=Menu  Alt+S=Save  Alt+N=New  Alt+F=Find  Alt+R=Replace  Alt+X=Cut  Alt+V=Paste");
//    pal_print(NP_C_RESET);
//}
//
///* -------- Edit area (rows 1 to NP_WIN_ROWS-2) -------- */
///*
//Fast path: build every visible row into a single stack buffer then
//emit it with one pal_print call.  The buffer layout is:
//  [colour_prefix][NP_WIN_COLS-1 chars of text/spaces][NUL]
//We reuse the same buffer for every row – only the text portion
//changes.  We never call np_goto inside the loop; after positioning
//at the first row the cursor advances naturally across each printed
//line.  Total write() calls = 1 (colour) + NP_EDIT_ROWS (rows)
//instead of 1 + NP_EDIT_ROWS*NP_WIN_COLS individual putchar calls.
//*/
//static void np_draw_edit(void)
//{
//    /* 32 = max ANSI prefix length, 8 = NUL + guard */
//    static char row_buf[32 + NP_WIN_COLS + 8];
//
//    int  safe = NP_WIN_COLS - 1;   /* cols we actually write per row */
//    int  clen = (int)pal_strlen(NP_C_EDIT);
//    int  row;
//
//    /* Stamp colour prefix once into the buffer start; it stays there
//       for every row because we always pal_print from row_buf[0]. */
//    pal_memcpy(row_buf, NP_C_EDIT, clen);
//
//    /* Position cursor at start of first edit row and hide it */
//    pal_hide_cursor();
//    np_goto(1, 0);
//
//    for (row = 0; row < NP_EDIT_ROWS; row++) {
//        int  dr = row + D.top;
//        char* p = row_buf + clen;   /* write text here */
//
//        if (dr < D.count) {
//            char* line = D.lines[dr];
//            int   len = (int)pal_strlen(line);
//            if (len > safe) len = safe;
//            /* copy visible text */
//            pal_memcpy(p, line, len);
//            /* replace non-printable / high bytes */
//            int i;
//            for (i = 0; i < len; i++)
//                if ((unsigned char)p[i] > 126 || (unsigned char)p[i] < 32)
//                    p[i] = '.';
//            /* pad remainder with spaces */
//            pal_memset(p + len, ' ', safe - len);
//        }
//        else {
//            /* past end of document */
//            p[0] = '~';
//            pal_memset(p + 1, ' ', safe - 1);
//        }
//
//        /* After the text, reposition to the start of the NEXT row.
//           We use \r\n so the terminal does the column reset for us –
//           no extra ESC sequence needed. */
//        p[safe] = '\r';
//        p[safe + 1] = '\n';
//        p[safe + 2] = '\0';
//
//        pal_print(row_buf);
//
//        /* After the first row is printed, suppress the colour prefix
//           for subsequent rows because the attribute is already set. */
//        if (row == 0) {
//            /* shift the text pointer to skip colour next iteration */
//            pal_memmove(row_buf, row_buf + clen, safe + 3);
//            /* adjust p offset for remaining rows */
//            clen = 0;
//        }
//    }
//
//    pal_print(NP_C_RESET);
//}
//
///* -------- Scroll viewport to keep cursor visible -------- */
//static void np_scroll(void)
//{
//    if (D.cy < D.top)
//        D.top = D.cy;
//    else if (D.cy >= D.top + NP_EDIT_ROWS)
//        D.top = D.cy - NP_EDIT_ROWS + 1;
//}
//
///* ================= DIRTY FLAGS ================= */
///*
//Bit flags controlling what np_redraw repaints.
//DIRTY_EDIT    – full edit area (scroll happened, file loaded, etc.)
//DIRTY_LINE    – only the current cursor line changed (single char edit)
//DIRTY_STATUS  – status bar only (cursor moved, modified flag changed)
//DIRTY_BAR     – ribbon bar only (menu open/close/navigate)
//DIRTY_DROP    – dropdown overlay (item selection changed)
//DIRTY_ALL     – everything (first draw, resize)
//*/
//#define NP_DIRTY_STATUS  0x01
//#define NP_DIRTY_LINE    0x02
//#define NP_DIRTY_EDIT    0x04
//#define NP_DIRTY_BAR     0x08
//#define NP_DIRTY_DROP    0x10
//#define NP_DIRTY_ALL     (NP_DIRTY_STATUS|NP_DIRTY_LINE|NP_DIRTY_EDIT|NP_DIRTY_BAR|NP_DIRTY_DROP)
//
///* Draw only one row of the edit area (0-based doc row dy) */
//static void np_draw_one_line(int dy)
//{
//    int safe = NP_WIN_COLS - 1;
//    int wr = 1 + (dy - D.top);   /* window row */
//    if (wr < 1 || wr > NP_EDIT_ROWS) return;
//
//    static char lb[32 + NP_WIN_COLS + 4];
//    int clen = (int)pal_strlen(NP_C_EDIT);
//    pal_memcpy(lb, NP_C_EDIT, clen);
//    char* p = lb + clen;
//
//    if (dy < D.count) {
//        char* line = D.lines[dy];
//        int   len = (int)pal_strlen(line);
//        if (len > safe) len = safe;
//        pal_memcpy(p, line, len);
//        int i;
//        for (i = 0; i < len; i++)
//            if ((unsigned char)p[i] > 126 || (unsigned char)p[i] < 32) p[i] = '.';
//        pal_memset(p + len, ' ', safe - len);
//    }
//    else {
//        p[0] = '~';
//        pal_memset(p + 1, ' ', safe - 1);
//    }
//    p[safe] = '\0';
//
//    np_goto(wr, 0);
//    pal_print(lb);
//    pal_print(NP_C_RESET);
//}
//
///* -------- Full redraw dispatcher -------- */
//static void np_redraw(int dirty)
//{
//    pal_hide_cursor();
//
//    if (dirty & NP_DIRTY_EDIT)   np_draw_edit();
//    else if (dirty & NP_DIRTY_LINE) np_draw_one_line(D.cy);
//
//    if (dirty & NP_DIRTY_BAR)    np_draw_bar();
//    if (dirty & NP_DIRTY_STATUS) np_draw_status();
//    if (dirty & NP_DIRTY_DROP) { if (D.menu_on == 2) np_draw_dropdown(D.sel_m); }
//
//    /* Always reposition hardware cursor */
//    int scr_row = 1 + (D.cy - D.top);
//    int scr_col = D.cx;
//    if (scr_col >= NP_WIN_COLS - 1) scr_col = NP_WIN_COLS - 2;
//    np_goto(scr_row, scr_col);
//    pal_show_cursor();
//}
//
///* ================= INLINE PROMPT ================= */
//
///* Render in status bar; result written into out (max maxlen chars) */
//static void np_prompt(const char* msg, char* out, int maxlen)
//{
//    pal_print(NP_C_MENU);
//    np_goto(NP_WIN_ROWS - 1, 0);
//    np_fill(NP_WIN_COLS);
//    np_goto(NP_WIN_ROWS - 1, 1);
//    pal_print(msg);
//    pal_print(NP_C_RESET);
//    pal_show_cursor();
//
//    int pos = 0;
//    int plen = (int)pal_strlen(msg);
//    out[0] = '\0';
//
//    while (1) {
//        int k = pal_raw_getkey();
//        if (k == 0)              continue;
//        if (k == '\r' || k == '\n') break;
//        if (k == '\033') { out[0] = '\0'; break; }
//        if (PAL_KEY_IS_BACKSPACE(k)) {
//            if (pos > 0) {
//                pos--;
//                out[pos] = '\0';
//                np_goto(NP_WIN_ROWS - 1, 1 + plen + pos);
//                pal_putchar(' ');
//                np_goto(NP_WIN_ROWS - 1, 1 + plen + pos);
//            }
//        }
//        else if (!PAL_KEY_IS_EXT(k) && k >= 32 && k <= 126 && pos < maxlen - 1) {
//            out[pos++] = (char)k;
//            out[pos] = '\0';
//            pal_putchar((char)k);
//        }
//    }
//    pal_hide_cursor();
//}
//
///* Flash a message in the status bar and wait for a keypress */
//static void np_msg(const char* msg)
//{
//    pal_print(NP_C_MENU);
//    np_goto(NP_WIN_ROWS - 1, 0);
//    np_fill(NP_WIN_COLS);
//    np_goto(NP_WIN_ROWS - 1, 1);
//    pal_print(msg);
//    pal_print(NP_C_RESET);
//    int k;
//    do { k = pal_raw_getkey(); } while (k == 0);
//}
//
///* ================= MENU DEFINITIONS ================= */
//
//static void np_init_menus(void)
//{
//    /* -------- File -------- */
//    pal_strcpy(menus[0].name, "File");
//    pal_strcpy(menus[0].items[0], "New");          menus[0].keys[0] = 'N';
//    pal_strcpy(menus[0].items[1], "Open...");      menus[0].keys[1] = 'O';
//    pal_strcpy(menus[0].items[2], "Save");         menus[0].keys[2] = 'S';
//    pal_strcpy(menus[0].items[3], "Save As...");   menus[0].keys[3] = 'A';
//    pal_strcpy(menus[0].items[4], "Exit");         menus[0].keys[4] = 'X';
//    menus[0].count = 5;
//
//    /* -------- Edit -------- */
//    pal_strcpy(menus[1].name, "Edit");
//    pal_strcpy(menus[1].items[0], "Cut");          menus[1].keys[0] = 't';
//    pal_strcpy(menus[1].items[1], "Copy");         menus[1].keys[1] = 'C';
//    pal_strcpy(menus[1].items[2], "Paste");        menus[1].keys[2] = 'P';
//    pal_strcpy(menus[1].items[3], "Clear");        menus[1].keys[3] = 'l';
//    menus[1].count = 4;
//
//    /* -------- Search -------- */
//    pal_strcpy(menus[2].name, "Search");
//    pal_strcpy(menus[2].items[0], "Find...");          menus[2].keys[0] = 'F';
//    pal_strcpy(menus[2].items[1], "Repeat Last Find"); menus[2].keys[1] = 'R';
//    pal_strcpy(menus[2].items[2], "Replace...");       menus[2].keys[2] = 'E';
//    menus[2].count = 3;
//
//    /* -------- Options -------- */
//    pal_strcpy(menus[3].name, "Options");
//    pal_strcpy(menus[3].items[0], "Display...");   menus[3].keys[0] = 'D';
//    pal_strcpy(menus[3].items[1], "Help Path..."); menus[3].keys[1] = 'H';
//    menus[3].count = 2;
//
//    /* -------- Help -------- */
//    pal_strcpy(menus[4].name, "Help");
//    pal_strcpy(menus[4].items[0], "Getting Started"); menus[4].keys[0] = 'G';
//    pal_strcpy(menus[4].items[1], "Keyboard");        menus[4].keys[1] = 'K';
//    pal_strcpy(menus[4].items[2], "About");           menus[4].keys[2] = 'A';
//    menus[4].count = 3;
//}
//
///* ================= MENU COMMANDS ================= */
//
//static void np_exec(int m, int it)
//{
//    char buf[NP_FNAME_MAX];
//    char find_buf[NP_SRCH_MAX];
//    char repl_buf[NP_SRCH_MAX];
//
//    if (m == 0) {
//
//        /* -------- File -------- */
//        if (it == 0) {                              /* New */
//            if (D.modified) {
//                np_prompt("Unsaved changes - discard? (y/n): ", buf, 4);
//                if (buf[0] != 'y' && buf[0] != 'Y') return;
//            }
//            np_doc_init();
//
//        }
//        else if (it == 1) {                       /* Open */
//            if (D.modified) {
//                np_prompt("Unsaved changes - open anyway? (y/n): ", buf, 4);
//                if (buf[0] != 'y' && buf[0] != 'Y') return;
//            }
//            np_prompt("Open file: ", buf, NP_FNAME_MAX);
//            if (buf[0]) {
//                if (!pal_file_exists(buf)) {
//                    np_msg(" Cannot open file - not found. Press any key.");
//                }
//                else {
//                    np_doc_init();
//                    np_load(buf);
//                }
//            }
//
//        }
//        else if (it == 2) {                       /* Save */
//            if (D.filename) {
//                np_save(D.filename);
//            }
//            else {
//                np_prompt("Save as: ", buf, NP_FNAME_MAX);
//                if (buf[0]) np_save(buf);
//            }
//
//        }
//        else if (it == 3) {                       /* Save As */
//            np_prompt("Save as: ", buf, NP_FNAME_MAX);
//            if (buf[0]) np_save(buf);
//
//        }
//        else if (it == 4) {                       /* Exit */
//            if (D.modified) {
//                np_prompt("File modified - exit anyway? (y/n): ", buf, 4);
//                if (buf[0] != 'y' && buf[0] != 'Y') return;
//            }
//            np_running = 0;
//        }
//
//    }
//    else if (m == 1) {
//
//        /* -------- Edit -------- */
//        if (it == 0) np_cut_line();
//        else if (it == 1) np_copy_line();
//        else if (it == 2) np_paste_line();
//        else if (it == 3) np_cut_line();
//
//    }
//    else if (m == 2) {
//
//        /* -------- Search -------- */
//        if (it == 0) {                              /* Find */
//            np_prompt("Find: ", find_buf, NP_SRCH_MAX);
//            if (find_buf[0]) {
//                pal_strncpy(last_search, find_buf, NP_SRCH_MAX);
//                int l = -1, c = 0;
//                int sx = D.cx + 1, sy = D.cy;
//                if (sx > (int)pal_strlen(D.lines[sy])) { sx = 0; sy++; }
//                if (!np_find_str(last_search, sy, sx, &l, &c))
//                    np_find_str(last_search, 0, 0, &l, &c);
//                if (l >= 0) { D.cy = l; D.cx = c; }
//                else np_msg(" String not found. Press any key.");
//            }
//
//        }
//        else if (it == 1) {                       /* Repeat Find */
//            if (last_search[0]) {
//                int l = -1, c = 0;
//                int sx = D.cx + 1, sy = D.cy;
//                if (sx > (int)pal_strlen(D.lines[sy])) { sx = 0; sy++; }
//                if (!np_find_str(last_search, sy, sx, &l, &c))
//                    np_find_str(last_search, 0, 0, &l, &c);
//                if (l >= 0) { D.cy = l; D.cx = c; }
//                else np_msg(" String not found. Press any key.");
//            }
//
//        }
//        else if (it == 2) {                       /* Replace */
//            np_prompt("Find: ", find_buf, NP_SRCH_MAX);
//            if (find_buf[0]) {
//                np_prompt("Replace with: ", repl_buf, NP_SRCH_MAX);
//                char mode[4];
//                np_prompt("Replace: (n)ext  (a)ll  ESC=cancel ", mode, 4);
//                if (mode[0] == 'a' || mode[0] == 'A') {
//                    int n = np_replace_all(find_buf, repl_buf);
//                    char nbuf[16];
//                    pal_itoa(n, nbuf);
//                    pal_print(NP_C_MENU);
//                    np_goto(NP_WIN_ROWS - 1, 0);
//                    np_fill(NP_WIN_COLS);
//                    np_goto(NP_WIN_ROWS - 1, 1);
//                    pal_print(nbuf);
//                    pal_print(" replacement(s) made. Press any key.");
//                    pal_print(NP_C_RESET);
//                    int k; do { k = pal_raw_getkey(); } while (k == 0);
//                }
//                else if (mode[0] == 'n' || mode[0] == 'N') {
//                    int l = -1, c = 0;
//                    if (!np_find_str(find_buf, D.cy, D.cx, &l, &c))
//                        np_find_str(find_buf, 0, 0, &l, &c);
//                    if (l >= 0) {
//                        np_replace_one(find_buf, repl_buf);
//                        pal_strncpy(last_search, find_buf, NP_SRCH_MAX);
//                    }
//                    else {
//                        np_msg(" String not found. Press any key.");
//                    }
//                }
//            }
//        }
//
//    }
//    else if (m == 3) {
//
//        /* -------- Options -------- */
//        np_msg(" Options not yet implemented. Press any key.");
//
//    }
//    else if (m == 4) {
//
//        /* -------- Help -------- */
//        if (it == 0)
//            np_msg(" F10=Menu  Arrows=Move  Alt+S=Save  Alt+N=New  Alt+F=Find. Press any key.");
//        else if (it == 1)
//            np_msg(" Alt+S=Save  Alt+N=New  Alt+F=Find  Alt+R=Replace  Alt+X=Cut  Alt+V=Paste. Press any key.");
//        else if (it == 2)
//            np_msg(" OE Notepad - PAL-native console text editor. Press any key.");
//    }
//}
//
///* ================= ALT KEY DETECTION ================= */
///*
//Linux  : Alt+key arrives as ESC byte + key byte.
//         pal_raw_getkey returns '\033' when the ESC has no
//         follow-up byte within the 100 ms timeout (bare ESC).
//         We re-read once: if another byte arrives quickly it
//         is the Alt combo character.
//Windows: Alt+key produces dedicated getch scan codes in the
//         0xFF00 range, mapped below.
//*/
//
///* Windows getch scan codes for common Alt+key combos */
//#define NP_ALT_SC_S  0x1F
//#define NP_ALT_SC_N  0x31
//#define NP_ALT_SC_F  0x21
//#define NP_ALT_SC_R  0x13
//#define NP_ALT_SC_X  0x2D
//#define NP_ALT_SC_C  0x2E
//#define NP_ALT_SC_V  0x2F
//
///* Map a key code to an Alt-combo letter (lowercase), or 0 if not an Alt key */
//static int np_alt_char(int k)
//{
//    /* Windows Alt scan codes */
//    if (k == (0xFF00 | NP_ALT_SC_S)) return 's';
//    if (k == (0xFF00 | NP_ALT_SC_N)) return 'n';
//    if (k == (0xFF00 | NP_ALT_SC_F)) return 'f';
//    if (k == (0xFF00 | NP_ALT_SC_R)) return 'r';
//    if (k == (0xFF00 | NP_ALT_SC_X)) return 'x';
//    if (k == (0xFF00 | NP_ALT_SC_C)) return 'c';
//    if (k == (0xFF00 | NP_ALT_SC_V)) return 'v';
//    return 0;
//}
//
///* ================= MAIN LOOP ================= */
//
//static void np_loop(void)
//{
//    np_running = 1;
//    int dirty = NP_DIRTY_ALL;   /* first frame: full draw */
//    int alt_pending = 0;
//    int prev_top = D.top;
//
//    while (np_running) {
//
//        /* ---- Scroll; if viewport shifted, edit area must be redrawn ---- */
//        np_scroll();
//        if (D.top != prev_top) { dirty |= NP_DIRTY_EDIT; prev_top = D.top; }
//
//        if (dirty) { np_redraw(dirty); dirty = 0; }
//
//        int k = pal_raw_getkey();
//        if (k == PAL_KEY_RESIZE) { dirty = NP_DIRTY_ALL; continue; }
//        if (k == 0) { continue; }
//
//        /* -------- Linux: detect Alt-prefix (ESC + key) -------- */
//        if (k == '\033' && !PAL_KEY_IS_EXT(k)) {
//            int k2 = pal_raw_getkey();
//            if (k2 == 0) {
//                D.menu_on = 0;
//                dirty = NP_DIRTY_BAR | NP_DIRTY_EDIT;
//                continue;
//            }
//            k = k2;
//            alt_pending = 1;
//        }
//
//        /* -------- F10: toggle ribbon -------- */
//        if (PAL_KEY_IS_EXT(k) && PAL_KEY_SC(k) == PAL_SC_F10) {
//            D.menu_on = D.menu_on ? 0 : 1;
//            D.sel_m = 0; D.sel_i = 0;
//            alt_pending = 0;
//            dirty = NP_DIRTY_BAR | NP_DIRTY_EDIT;
//            continue;
//        }
//
//        /* -------- Alt shortcuts -------- */
//        {
//            int ac = alt_pending ? k : np_alt_char(k);
//            if (ac) {
//                alt_pending = 0;
//                if (ac >= 'A' && ac <= 'Z') ac += 32;
//                switch (ac) {
//                case 's': np_exec(0, 2); break;
//                case 'n': np_exec(0, 0); break;
//                case 'f': np_exec(2, 0); break;
//                case 'r': np_exec(2, 2); break;
//                case 'x': np_exec(1, 0); break;
//                case 'c': np_exec(1, 1); break;
//                case 'v': np_exec(1, 2); break;
//                default:  break;
//                }
//                dirty = NP_DIRTY_ALL;
//                continue;
//            }
//        }
//        alt_pending = 0;
//
//        /* -------- Menu navigation -------- */
//        if (D.menu_on) {
//            if (PAL_KEY_IS_EXT(k)) {
//                int sc = PAL_KEY_SC(k);
//                if (D.menu_on == 1) {
//                    if (sc == PAL_SC_LEFT)  D.sel_m = (D.sel_m - 1 + NP_MENU_COUNT) % NP_MENU_COUNT;
//                    else if (sc == PAL_SC_RIGHT) D.sel_m = (D.sel_m + 1) % NP_MENU_COUNT;
//                    else if (sc == PAL_SC_DOWN || sc == PAL_SC_UP) { D.menu_on = 2; D.sel_i = 0; }
//                }
//                else {
//                    if (sc == PAL_SC_UP)    D.sel_i = (D.sel_i - 1 + menus[D.sel_m].count) % menus[D.sel_m].count;
//                    else if (sc == PAL_SC_DOWN)  D.sel_i = (D.sel_i + 1) % menus[D.sel_m].count;
//                    else if (sc == PAL_SC_LEFT) { D.sel_m = (D.sel_m - 1 + NP_MENU_COUNT) % NP_MENU_COUNT; D.sel_i = 0; }
//                    else if (sc == PAL_SC_RIGHT) { D.sel_m = (D.sel_m + 1) % NP_MENU_COUNT;                  D.sel_i = 0; }
//                }
//                dirty = NP_DIRTY_BAR | NP_DIRTY_DROP;
//            }
//            else if (k == '\r' || k == '\n') {
//                if (D.menu_on == 2) { np_exec(D.sel_m, D.sel_i); D.menu_on = 0; }
//                else { D.menu_on = 2; D.sel_i = 0; }
//                dirty = NP_DIRTY_ALL;
//            }
//            else if (k == '\033') {
//                D.menu_on = 0;
//                dirty = NP_DIRTY_BAR | NP_DIRTY_EDIT;
//            }
//            else {
//                if (D.menu_on == 2 && k >= 32 && k <= 126) {
//                    char lk = (char)k;
//                    if (lk >= 'A' && lk <= 'Z') lk += 32;
//                    int i;
//                    for (i = 0; i < menus[D.sel_m].count; i++) {
//                        char mk = menus[D.sel_m].keys[i];
//                        if (mk >= 'A' && mk <= 'Z') mk += 32;
//                        if (mk == lk) { np_exec(D.sel_m, i); D.menu_on = 0; break; }
//                    }
//                    dirty = NP_DIRTY_ALL;
//                }
//            }
//            continue;
//        }
//
//        /* -------- Normal text editing -------- */
//        if (PAL_KEY_IS_EXT(k)) {
//            int sc = PAL_KEY_SC(k);
//            int old_cy = D.cy;
//            switch (sc) {
//            case PAL_SC_UP: case PAL_SC_DOWN:
//            case PAL_SC_LEFT: case PAL_SC_RIGHT:
//            case PAL_SC_HOME: case PAL_SC_END:
//                np_move(sc);
//                dirty = NP_DIRTY_STATUS;   /* only status bar + cursor */
//                break;
//            case PAL_SC_PGUP: case PAL_SC_PGDN:
//                np_move(sc);
//                dirty = NP_DIRTY_EDIT | NP_DIRTY_STATUS;
//                break;
//            case PAL_SC_DEL:
//                np_delete();
//                dirty = (D.cy != old_cy ? NP_DIRTY_EDIT : NP_DIRTY_LINE) | NP_DIRTY_STATUS;
//                break;
//            default: break;
//            }
//        }
//        else if (k == '\r' || k == '\n') {
//            np_newline();
//            dirty = NP_DIRTY_EDIT | NP_DIRTY_STATUS;
//
//        }
//        else if (k == '\t') {
//            int i;
//            for (i = 0; i < NP_TAB_STOP; i++) np_insert(' ');
//            dirty = NP_DIRTY_LINE | NP_DIRTY_STATUS;
//
//        }
//        else if (PAL_KEY_IS_BACKSPACE(k)) {
//            int old_cy = D.cy;
//            np_backspace();
//            dirty = (D.cy != old_cy ? NP_DIRTY_EDIT : NP_DIRTY_LINE) | NP_DIRTY_STATUS;
//
//        }
//        else if (k >= 32 && k <= 126) {
//            np_insert((char)k);
//            dirty = NP_DIRTY_LINE | NP_DIRTY_STATUS;   /* only current line changed */
//        }
//    }
//}
//
///* ================= ENTRY POINT ================= */
//
//void oe_notepad_run(const char* filename)
//{
//	pal_clear_screen();
//    /* -------- Initialise globals -------- */
//    pal_memset(&D, 0, (int)sizeof(D));
//    pal_memset(menus, 0, (int)sizeof(menus));
//    pal_memset(last_search, 0, (int)sizeof(last_search));
//    pal_memset(np_clip, 0, (int)sizeof(np_clip));
//
//    np_init_menus();
//    np_doc_init();
//
//    if (filename && filename[0] != '\0')
//        np_load(filename);
//
//    /* -------- Enter raw mode -------- */
//    pal_raw_enter();
//
//    /* -------- Paint the notepad window area black in one pass -------- */
//    {
//        static char blank[32 + NP_WIN_COLS + 4];
//        int clen = (int)pal_strlen(NP_C_EDIT);
//        pal_memcpy(blank, NP_C_EDIT, clen);
//        pal_memset(blank + clen, ' ', NP_WIN_COLS);
//        blank[clen + NP_WIN_COLS] = '\0';
//        int row;
//        for (row = 0; row < NP_WIN_ROWS; row++) {
//            np_goto(row, 0);
//            pal_print(blank);
//        }
//        pal_print(NP_C_RESET);
//    }
//
//    /* -------- Run -------- */
//    np_loop();
//
//    /* -------- Leave raw mode -------- */
//    pal_raw_exit();
//
//    /* -------- Free document -------- */
//    np_doc_free();
//
//    /* -------- Restore OE frame -------- */
//    ui_init();
//}