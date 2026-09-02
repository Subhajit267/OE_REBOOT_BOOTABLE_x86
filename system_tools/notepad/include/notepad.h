/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-10
Date Last Modified: 2026-03-10
Module: Applications
File: notepad.h
About: OE integrated text editor (notepad).
       Menu-bar ribbon activated by F10, just like the
       original standalone editor. Fully self-contained
       platform layer inside notepad.c – no direct stdio
       or termios calls leak into the rest of OE.
Key bindings:
       F10             - Open / close menu ribbon
       Arrow keys      - Navigate edit area or menu
       Enter           - Confirm menu item
       ESC             - Close menu / cancel prompt
       Ctrl+S          - Save
       Ctrl+N          - New file
       Ctrl+F          - Find
       Ctrl+R          - Replace
       Ctrl+X          - Cut line
       Ctrl+C          - Copy line
       Ctrl+V          - Paste line
       Home / End      - Start / end of line
       PgUp / PgDn     - Scroll page
       Backspace / Del - Delete characters
Revisions:
- 2026-03-10  Initial creation – menu ribbon, F10, full editor
------------------------------------------------------------
*/

#ifndef OE_NOTEPAD_H
#define OE_NOTEPAD_H

/*
------------------------------------------------------------
Function : oe_notepad_run
Purpose  : Launch the OE text editor.
           filename – path to open immediately, or NULL / ""
           for a blank document.
           Returns when the user exits. Restores the OE UI
           frame (ui_init) before returning.
------------------------------------------------------------
*/
void oe_notepad_run(const char* filename);

#endif