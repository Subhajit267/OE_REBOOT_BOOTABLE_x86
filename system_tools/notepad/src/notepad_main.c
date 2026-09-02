/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-10
Date Last Modified: 2026-03-10
Module: Applications
File: notepad_main.c
About: Main controller for the OE notepad. Contains global
       variable definitions, menu setup, command dispatch,
       Alt-key detection, main event loop, and the entry
       point oe_notepad_run.
Revisions:
- 2026-03-10  Extracted from monolithic notepad.c
- 2026-03-10  Use np_init_background() for initial paint
- 2026-03-10  Replace direct np_goto/np_fill with np_msg
- 2026-08-26  BUG FIX: the "Help -> Keyboard" np_msg() was ~89
              visible chars printed inside a 78-col window with no
              wrap — guaranteed overflow. Shortened to fit one line.
------------------------------------------------------------
*/
//notepad line wrap and scrolling reqd
// Command-Bar wrap
#include "notepad.h"
#include "notepad_priv.h"
#include "notepad_edit.h"
#include "notepad_view.h"
#include "pal.h"
#include "ui_setup.h"

/* ================= GLOBALS ================= */
NP_Doc   D;
NP_Menu  menus[NP_MENU_COUNT];
char     last_search[NP_SRCH_MAX];
char     np_clip[NP_CLIP_MAX];
int      np_running;

/* ================= MENU DEFINITIONS ================= */
static void np_init_menus(void)
{
    pal_strcpy(menus[0].name, "File");
    pal_strcpy(menus[0].items[0], "New");          menus[0].keys[0] = 'N';
    pal_strcpy(menus[0].items[1], "Open...");      menus[0].keys[1] = 'O';
    pal_strcpy(menus[0].items[2], "Save");         menus[0].keys[2] = 'S';
    pal_strcpy(menus[0].items[3], "Save As...");   menus[0].keys[3] = 'A';
    pal_strcpy(menus[0].items[4], "Exit");         menus[0].keys[4] = 'X';
    menus[0].count = 5;

    pal_strcpy(menus[1].name, "Edit");
    pal_strcpy(menus[1].items[0], "Cut");          menus[1].keys[0] = 't';
    pal_strcpy(menus[1].items[1], "Copy");         menus[1].keys[1] = 'C';
    pal_strcpy(menus[1].items[2], "Paste");        menus[1].keys[2] = 'P';
    pal_strcpy(menus[1].items[3], "Clear");        menus[1].keys[3] = 'l';
    menus[1].count = 4;

    pal_strcpy(menus[2].name, "Search");
    pal_strcpy(menus[2].items[0], "Find...");          menus[2].keys[0] = 'F';
    pal_strcpy(menus[2].items[1], "Repeat Last Find"); menus[2].keys[1] = 'R';
    pal_strcpy(menus[2].items[2], "Replace...");       menus[2].keys[2] = 'E';
    menus[2].count = 3;

    pal_strcpy(menus[3].name, "Options");
    pal_strcpy(menus[3].items[0], "Display...");   menus[3].keys[0] = 'D';
    pal_strcpy(menus[3].items[1], "Help Path..."); menus[3].keys[1] = 'H';
    menus[3].count = 2;

    pal_strcpy(menus[4].name, "Help");
    pal_strcpy(menus[4].items[0], "Getting Started"); menus[4].keys[0] = 'G';
    pal_strcpy(menus[4].items[1], "Keyboard");        menus[4].keys[1] = 'K';
    pal_strcpy(menus[4].items[2], "About");           menus[4].keys[2] = 'A';
    menus[4].count = 3;
}

/* ================= MENU COMMANDS ================= */
static void np_exec(int m, int it)
{
    char buf[NP_FNAME_MAX];
    char find_buf[NP_SRCH_MAX];
    char repl_buf[NP_SRCH_MAX];

    if (m == 0) {
        if (it == 0) {
            if (D.modified) {
                np_prompt("Unsaved changes - discard? (y/n): ", buf, 4);
                if (buf[0] != 'y' && buf[0] != 'Y') return;
            }
            np_doc_init();
        }
        else if (it == 1) {
            if (D.modified) {
                np_prompt("Unsaved changes - open anyway? (y/n): ", buf, 4);
                if (buf[0] != 'y' && buf[0] != 'Y') return;
            }
            np_prompt("Open file: ", buf, NP_FNAME_MAX);
            if (buf[0]) {
                if (!pal_file_exists(buf)) {
                    np_msg(" Cannot open file - not found. Press any key.");
                }
                else {
                    np_doc_init();
                    np_load(buf);
                }
            }
        }
        else if (it == 2) {
            if (D.filename) {
                np_save(D.filename);
            }
            else {
                np_prompt("Save as: ", buf, NP_FNAME_MAX);
                if (buf[0]) np_save(buf);
            }
        }
        else if (it == 3) {
            np_prompt("Save as: ", buf, NP_FNAME_MAX);
            if (buf[0]) np_save(buf);
        }
        else if (it == 4) {
            if (D.modified) {
                np_prompt("File modified - exit anyway? (y/n): ", buf, 4);
                if (buf[0] != 'y' && buf[0] != 'Y') return;
            }
            np_running = 0;
        }
    }
    else if (m == 1) {
        if (it == 0) np_cut_line();
        else if (it == 1) np_copy_line();
        else if (it == 2) np_paste_line();
        else if (it == 3) np_cut_line();
    }
    else if (m == 2) {
        if (it == 0) {
            np_prompt("Find: ", find_buf, NP_SRCH_MAX);
            if (find_buf[0]) {
                pal_strncpy(last_search, find_buf, NP_SRCH_MAX);
                int l = -1, c = 0;
                int sx = D.cx + 1, sy = D.cy;
                if (sx > (int)pal_strlen(D.lines[sy])) { sx = 0; sy++; }
                if (!np_find_str(last_search, sy, sx, &l, &c))
                    np_find_str(last_search, 0, 0, &l, &c);
                if (l >= 0) { D.cy = l; D.cx = c; }
                else np_msg(" String not found. Press any key.");
            }
        }
        else if (it == 1) {
            if (last_search[0]) {
                int l = -1, c = 0;
                int sx = D.cx + 1, sy = D.cy;
                if (sx > (int)pal_strlen(D.lines[sy])) { sx = 0; sy++; }
                if (!np_find_str(last_search, sy, sx, &l, &c))
                    np_find_str(last_search, 0, 0, &l, &c);
                if (l >= 0) { D.cy = l; D.cx = c; }
                else np_msg(" String not found. Press any key.");
            }
        }
        else if (it == 2) {
            np_prompt("Find: ", find_buf, NP_SRCH_MAX);
            if (find_buf[0]) {
                np_prompt("Replace with: ", repl_buf, NP_SRCH_MAX);
                char mode[4];
                np_prompt("Replace: (n)ext  (a)ll  ESC=cancel ", mode, 4);
                if (mode[0] == 'a' || mode[0] == 'A') {
                    int n = np_replace_all(find_buf, repl_buf);
                    char nbuf[16];
                    pal_itoa(n, nbuf);
                    /* Build a single message and use np_msg */
                    char fullmsg[64];
                    pal_strcpy(fullmsg, nbuf);
                    pal_strcat(fullmsg, " replacement(s) made. Press any key.");
                    np_msg(fullmsg);
                }
                else if (mode[0] == 'n' || mode[0] == 'N') {
                    int l = -1, c = 0;
                    if (!np_find_str(find_buf, D.cy, D.cx, &l, &c))
                        np_find_str(find_buf, 0, 0, &l, &c);
                    if (l >= 0) {
                        np_replace_one(find_buf, repl_buf);
                        pal_strncpy(last_search, find_buf, NP_SRCH_MAX);
                    }
                    else {
                        np_msg(" String not found. Press any key.");
                    }
                }
            }
        }
    }
    else if (m == 3) {
        np_msg(" Options not yet implemented. Press any key.");
    }
    else if (m == 4) {
        if (it == 0)
            np_msg(" F10=Menu  Arrows=Move  Alt+S=Save  Alt+N=New  Alt+F=Find. Press any key.");
        else if (it == 1)
            np_msg(" Alt+S=Save Alt+N=New Alt+F=Find Alt+R=Replace Alt+X=Cut Alt+V=Paste");
        else if (it == 2)
            np_msg(" OE Notepad V2.09 - PAL-native console text editor. Press any key.");
    }
}

/* ================= ALT KEY DETECTION ================= */
static int np_alt_char(int k)
{
    if (k == (0xFF00 | 0x1F)) return 's';
    if (k == (0xFF00 | 0x31)) return 'n';
    if (k == (0xFF00 | 0x21)) return 'f';
    if (k == (0xFF00 | 0x13)) return 'r';
    if (k == (0xFF00 | 0x2D)) return 'x';
    if (k == (0xFF00 | 0x2E)) return 'c';
    if (k == (0xFF00 | 0x2F)) return 'v';
    return 0;
}

/* ================= MAIN LOOP ================= */
static void np_loop(void)
{
    np_running = 1;
    int dirty = NP_DIRTY_ALL;
    int alt_pending = 0;
    int prev_top = D.top;

    while (np_running) {
        np_scroll();
        if (D.top != prev_top) { dirty |= NP_DIRTY_EDIT; prev_top = D.top; }

        if (dirty) { np_redraw(dirty); dirty = 0; }

        int k = pal_raw_getkey();
        if (k == PAL_KEY_RESIZE) { dirty = NP_DIRTY_ALL; continue; }
        if (k == 0) { continue; }

        if (k == '\033' && !PAL_KEY_IS_EXT(k)) {
            int k2 = pal_raw_getkey();
            if (k2 == 0) {
                D.menu_on = 0;
                dirty = NP_DIRTY_BAR | NP_DIRTY_EDIT;
                continue;
            }
            k = k2;
            alt_pending = 1;
        }

        if (PAL_KEY_IS_EXT(k) && PAL_KEY_SC(k) == PAL_SC_F10) {
            D.menu_on = D.menu_on ? 0 : 1;
            D.sel_m = 0; D.sel_i = 0;
            alt_pending = 0;
            dirty = NP_DIRTY_BAR | NP_DIRTY_EDIT;
            continue;
        }

        {
            int ac = alt_pending ? k : np_alt_char(k);
            if (ac) {
                alt_pending = 0;
                if (ac >= 'A' && ac <= 'Z') ac += 32;
                switch (ac) {
                case 's': np_exec(0, 2); break;
                case 'n': np_exec(0, 0); break;
                case 'f': np_exec(2, 0); break;
                case 'r': np_exec(2, 2); break;
                case 'x': np_exec(1, 0); break;
                case 'c': np_exec(1, 1); break;
                case 'v': np_exec(1, 2); break;
                default:  break;
                }
                dirty = NP_DIRTY_ALL;
                continue;
            }
        }
        alt_pending = 0;

        if (D.menu_on) {
            if (PAL_KEY_IS_EXT(k)) {
                int sc = PAL_KEY_SC(k);
                if (D.menu_on == 1) {
                    if (sc == PAL_SC_LEFT)  D.sel_m = (D.sel_m - 1 + NP_MENU_COUNT) % NP_MENU_COUNT;
                    else if (sc == PAL_SC_RIGHT) D.sel_m = (D.sel_m + 1) % NP_MENU_COUNT;
                    else if (sc == PAL_SC_DOWN || sc == PAL_SC_UP) { D.menu_on = 2; D.sel_i = 0; }
                }
                else {
                    if (sc == PAL_SC_UP)    D.sel_i = (D.sel_i - 1 + menus[D.sel_m].count) % menus[D.sel_m].count;
                    else if (sc == PAL_SC_DOWN)  D.sel_i = (D.sel_i + 1) % menus[D.sel_m].count;
                    else if (sc == PAL_SC_LEFT) { D.sel_m = (D.sel_m - 1 + NP_MENU_COUNT) % NP_MENU_COUNT; D.sel_i = 0; }
                    else if (sc == PAL_SC_RIGHT) { D.sel_m = (D.sel_m + 1) % NP_MENU_COUNT;                  D.sel_i = 0; }
                }
                dirty = NP_DIRTY_BAR | NP_DIRTY_DROP;
            }
            else if (k == '\r' || k == '\n') {
                if (D.menu_on == 2) { np_exec(D.sel_m, D.sel_i); D.menu_on = 0; }
                else { D.menu_on = 2; D.sel_i = 0; }
                dirty = NP_DIRTY_ALL;
            }
            else if (k == '\033') {
                D.menu_on = 0;
                dirty = NP_DIRTY_BAR | NP_DIRTY_EDIT;
            }
            else {
                if (D.menu_on == 2 && k >= 32 && k <= 126) {
                    char lk = (char)k;
                    if (lk >= 'A' && lk <= 'Z') lk += 32;
                    int i;
                    for (i = 0; i < menus[D.sel_m].count; i++) {
                        char mk = menus[D.sel_m].keys[i];
                        if (mk >= 'A' && mk <= 'Z') mk += 32;
                        if (mk == lk) { np_exec(D.sel_m, i); D.menu_on = 0; break; }
                    }
                    dirty = NP_DIRTY_ALL;
                }
            }
            continue;
        }

        if (PAL_KEY_IS_EXT(k)) {
            int sc = PAL_KEY_SC(k);
            int old_cy = D.cy;
            switch (sc) {
            case PAL_SC_UP: case PAL_SC_DOWN:
            case PAL_SC_LEFT: case PAL_SC_RIGHT:
            case PAL_SC_HOME: case PAL_SC_END:
                np_move(sc);
                dirty = NP_DIRTY_STATUS;
                break;
            case PAL_SC_PGUP: case PAL_SC_PGDN:
                np_move(sc);
                dirty = NP_DIRTY_EDIT | NP_DIRTY_STATUS;
                break;
            case PAL_SC_DEL:
                np_delete();
                dirty = (D.cy != old_cy ? NP_DIRTY_EDIT : NP_DIRTY_LINE) | NP_DIRTY_STATUS;
                break;
            default: break;
            }
        }
        else if (k == '\r' || k == '\n') {
            np_newline();
            dirty = NP_DIRTY_EDIT | NP_DIRTY_STATUS;
        }
        else if (k == '\t') {
            int i;
            for (i = 0; i < NP_TAB_STOP; i++) np_insert(' ');
            dirty = NP_DIRTY_LINE | NP_DIRTY_STATUS;
        }
        else if (PAL_KEY_IS_BACKSPACE(k)) {
            int old_cy = D.cy;
            np_backspace();
            dirty = (D.cy != old_cy ? NP_DIRTY_EDIT : NP_DIRTY_LINE) | NP_DIRTY_STATUS;
        }
        else if (k >= 32 && k <= 126) {
            np_insert((char)k);
            dirty = NP_DIRTY_LINE | NP_DIRTY_STATUS;
        }
    }
}

/* ================= ENTRY POINT ================= */
void oe_notepad_run(const char* filename)
{
    pal_clear_screen();

    pal_memset(&D, 0, (int)sizeof(D));
    pal_memset(menus, 0, (int)sizeof(menus));
    pal_memset(last_search, 0, (int)sizeof(last_search));
    pal_memset(np_clip, 0, (int)sizeof(np_clip));

    np_init_menus();
    np_doc_init();

    if (filename && filename[0] != '\0')
        np_load(filename);

    pal_raw_enter();

    /* Paint the initial background using the view module */
    np_init_background();

    np_loop();

    pal_raw_exit();
    np_doc_free();
    ui_init();
}