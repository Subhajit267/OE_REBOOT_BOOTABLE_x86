/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-16
Date Last Modified: 2026-03-16
Module: UI
File: ui_coordinates.h
About: Centralised coordinate constants for OE 80x25 VGA grid.
       Every row/column value used across the entire UI codebase
       is defined here. Change once, everything updates.

       Include via ui_elements.h — already included everywhere.
       Do NOT include directly in individual modules.

       80x25 Visual Layout:
       Row  1  [=== TOP BORDER ==========================================]
       Row  2  [       Operating Environment  ver: X.X.X                ]  centered
       Row  3  [          (Pre Release Build C and PAL Based)           ]  centered
       Row  4  [ =============== |                                      ]  logo row 1
       Row  5  [|-  -----  ----  | CONTENT col 19                       ]  logo row 2
       Row  6  [| |     | |      | CONTENT col 19                       ]  logo row 3
       Row  7  [| |     |  ===   | CONTENT col 19                       ]  logo row 4
       Row  8  [| |     | |      | CONTENT col 19                       ]  logo row 5
       Row  9  [|-  -----  ----  | CONTENT col 19                       ]  logo row 6
       Row 10  [ =============== |                                      ]  logo row 7
       Row 11  [                   CONTENT col 19 <- same col, logo gone]
       Row 12  [                   CONTENT col 19                       ]
       ...
       Row 22  [                   CONTENT col 19                       ]
       Row 23  [ STATUS: _____________                                  ]
       Row 24  [                                                        ]  blank buffer
       Row 25  [=== BOTTOM BORDER ======================================]

       Column rules:
         Logo screens  -> content ALWAYS col 19, even after logo ends
         No-logo screens (calculator, sysinfo, notepad) -> col 2
         Submenu/extra -> col 21
         Right edge    -> col 79 (1 gap from border)

Revisions:
- 2026-03-16  Initial implementation for 80x25 kernel target
------------------------------------------------------------
*/

#ifndef UI_COORDINATES_H
#define UI_COORDINATES_H

/* ================================================================
   SECTION 1 - GRID DIMENSIONS
   ================================================================ */

#define UI_COLS                  80
#define UI_ROWS                  25

   /* ================================================================
      SECTION 2 - FIXED STRUCTURAL ROWS
      layout() owns these rows. No module writes here.
      ================================================================ */

#define UI_ROW_TOP_BORDER         1
#define UI_ROW_OE_NAME            2    /* "Operating Environment  ver: X.X.X" centered */
#define UI_ROW_BUILD_TYPE         3    /* "(Pre Release Build C and PAL Based)" centered */
#define UI_ROW_BOTTOM_BORDER     25

      /* ================================================================
         SECTION 3 - LOGO
         Drawn by logo() in ui_elements.c.
         Occupies rows 5-11, cols 2-17.
         No module writes to this zone except logo().
         ================================================================ */

#define UI_LOGO_ROW_START         6    /* first row of logo (=== top line) */
#define UI_LOGO_ROW_END          12    /* last row of logo (=== bottom line) */
#define UI_LOGO_COL_START         4
#define UI_LOGO_COL_END          19
#define UI_LOGO_WIDTH            16
#define UI_LOGO_HEIGHT            7

         /* ================================================================
            SECTION 4 - CONTENT COLUMNS
            ================================================================ */

#define UI_COL_LOGO              19    /* content col for ALL logo screens — always, even after logo ends */
#define UI_COL_NO_LOGO            2    /* content col for no-logo screens (calculator, sysinfo, notepad) */
#define UI_COL_SUBMENU           21    /* indented content for submenus */
#define UI_COL_RIGHT             79    /* rightmost content col */

            /* ================================================================
               SECTION 5 - CONTENT ROWS
               ================================================================ */

#define UI_ROW_CONTENT_START      5    /* first row with content beside logo */
#define UI_ROW_LOGO_END          10    /* logo last row */
#define UI_ROW_AFTER_LOGO        11    /* first row below logo, still col 19 */
#define UI_ROW_CONTENT_END       22    /* last content row */
#define UI_ROW_STATUS            23    /* status bar - ui_status() only */
#define UI_ROW_BUFFER            24    /* blank buffer */

               /* ================================================================
                  SECTION 6 - STATUS BAR
                  ui_status() always writes here. Nothing else uses row 23.
                  ================================================================ */

#define UI_STATUS_ROW            23
#define UI_STATUS_COL            12

                  /* ================================================================
                     SECTION 7 - PROMPT SHELL
                     ================================================================ */

#define UI_PROMPT_VER_ROW         5    /* version string — beside logo */
#define UI_PROMPT_VER_COL        19
#define UI_PROMPT_WELCOME_COL    56    /* "Welcome " right side */
#define UI_PROMPT_USER_COL       64   /* username */
#define UI_PROMPT_MSG_ROW         6    /* hint or guest message */
#define UI_PROMPT_MSG_COL        19
#define UI_PROMPT_CMD_ROW         8    /* "Command->" label */
#define UI_PROMPT_CMD_COL        19
#define UI_PROMPT_INPUT_COL      28    /* cursor after "Command->" */
#define UI_PROMPT_OUT_ROW        11    /* output — after logo ends, col 19 */
#define UI_PROMPT_OUT_COL        19
#define UI_PROMPT_SHUTDOWN_ROW   11
#define UI_PROMPT_SHUTDOWN_COL   19
#define UI_PROMPT_INST_ROW       11
#define UI_PROMPT_INST_COL       19

                     /* ================================================================
                        SECTION 8 - VER COMMAND
                        ================================================================ */

#define UI_VER_ROW               10    /* version line — beside logo */
#define UI_VER_COL               19
#define UI_BUILD_DETAILS_ROW     11
#define UI_BUILD_DETAILS_COL     19
#define UI_BUILD_TYPE_ROW        12
#define UI_BUILD_TYPE_COL        21
#define UI_BUILD_DT_ROW          13
#define UI_BUILD_DT_COL          21
#define UI_BUILD_DEV_ROW         14
#define UI_BUILD_DEV_COL         21
#define UI_BUILD_COPYRIGHT_ROW   15
#define UI_BUILD_COPYRIGHT_COL   18
                        /* build details = UI_VER_ROW + 1  */
                        /* developer     = UI_VER_ROW + 2  */

                        /* ================================================================
                           SECTION 9 - BOOTSCREEN
                           ================================================================ */

#define UI_BOOT_APP_ROW           5    /* app name beside logo */
#define UI_BOOT_APP_COL          19
#define UI_BOOT_VER_ROW           6
#define UI_BOOT_VER_COL          19
#define UI_BOOT_WAIT_ROW         12    /* "Please_Wait." — after logo */
#define UI_BOOT_WAIT_COL         35
#define UI_BOOT_LOAD_ROW         13    /* "LOADING......" */
#define UI_BOOT_LOAD_COL         35
#define UI_PROGRESS_ROW          15    /* progress bar */
#define UI_PROGRESS_COL          20
#define UI_PROGRESS_WIDTH        40

                           /* ================================================================
                              SECTION 10 - LOGIN
                              ================================================================ */

#define UI_LOGIN_MSG_ROW          5    /* "To begin press enter..." beside logo */
#define UI_LOGIN_MSG_COL         19
#define UI_LOGIN_USERID_ROW       7
#define UI_LOGIN_USERID_COL      19
#define UI_LOGIN_PASS_ROW         8
#define UI_LOGIN_PASS_COL        19
#define UI_LOGIN_GUEST_MSG_ROW    5
#define UI_LOGIN_GUEST_MSG_COL   19
#define UI_LOGIN_GUEST_ID_ROW     7
#define UI_LOGIN_GUEST_ID_COL    19

                              /* ================================================================
                                 SECTION 11 - INSTALLER
                                 ================================================================ */

#define UI_INST_TITLE_ROW         4    /* long question — before logo starts */
#define UI_INST_TITLE_COL        19
#define UI_INST_BODY_ROW          5    /* beside logo */
#define UI_INST_BODY_COL         19
#define UI_INST_HELP_ROW          5    /* "Help Me Decide:" */
#define UI_INST_HELP_COL         19
#define UI_INST_LINES_ROW         6    /* explanation lines */
#define UI_INST_LINES_COL        19
#define UI_INST_CHOICE_ROW       11    /* "Enter your choice(y/n):" after logo */
#define UI_INST_CHOICE_COL       19

                                 /* ================================================================
                                    SECTION 12 - SETTINGS
                                    BASE_ROW/BASE_COL in settings.c replace with these.
                                    ================================================================ */

#define UI_SETTINGS_ROW           5    /* title beside logo */
#define UI_SETTINGS_COL          19
#define UI_COLOR_TABLE_ROW        8    /* color table starts beside logo */
#define UI_COLOR_TABLE_COL       19

                                    /* ================================================================
                                       SECTION 13 - REGISTRY EDITOR
                                       row_base/col_base in regedit.c replace with these.
                                       ================================================================ */

#define UI_REGEDIT_ROW            5    /* title beside logo */
#define UI_REGEDIT_COL           19
#define UI_REGEDIT_CMD_ROW       12    /* "Command->" after logo */
#define UI_REGEDIT_CMD_COL       19
#define UI_REGEDIT_INPUT_COL     30

                                       
                                          /* ================================================================
                                             SECTION 14 - SYSTEM INFO  (no logo, full width box)
                                             ================================================================ */

#define UI_SYSINFO_BOX_COL        19    /* left edge of box */
#define UI_SYSINFO_BOX_WIDTH      58    /* cols 19-76 */
#define UI_SYSINFO_RB             (UI_SYSINFO_BOX_COL + UI_SYSINFO_BOX_WIDTH - 1) /* 76 */

                                             /* Box content columns */
#define UI_SYSINFO_LBL_COL        (UI_SYSINFO_BOX_COL + 2)   /* 21 – label */
#define UI_SYSINFO_VAL_COL        (UI_SYSINFO_LBL_COL + 14)  /* 35 – value */
#define UI_SYSINFO_UNIT_COL       (UI_SYSINFO_VAL_COL + 8)   /* 43 – unit */

/* Filler Rows */
#define UI_SYSINFO_Filler1_ROW    20
#define UI_SYSINFO_Filler2_ROW    21

/* Box borders (shared across pages) */
#define UI_SYSINFO_TOP_ROW        5
#define UI_SYSINFO_TITLE_ROW      6
#define UI_SYSINFO_TITLE2_ROW     7
#define UI_SYSINFO_BOT_ROW        22

/* Page 1 rows */
#define UI_SYSINFO_P1_SECT1_ROW   8    /* OE BUILD DETAILS section header */
#define UI_SYSINFO_P1_VER_ROW     11
#define UI_SYSINFO_P1_TYPE_ROW    12
#define UI_SYSINFO_P1_DATE_ROW    13
#define UI_SYSINFO_P1_DEV_ROW     14
#define UI_SYSINFO_P1_USER_ROW    15
#define UI_SYSINFO_P1_SECT2_ROW   16   /* PAL RUNTIME section header */
#define UI_SYSINFO_P1_BACKEND_ROW 19


/* Page 2 rows */
#define UI_SYSINFO_P2_SECT1_ROW   8    /* CPU INFORMATION */
#define UI_SYSINFO_P2_MODEL_ROW   11
#define UI_SYSINFO_P2_CORES_ROW   12
#define UI_SYSINFO_P2_THREADS_ROW 13
#define UI_SYSINFO_P2_MHZ_ROW     14
#define UI_SYSINFO_P2_ARCH_ROW    15
#define UI_SYSINFO_P2_SECT2_ROW   16   /* SYSTEM UPTIME */
#define UI_SYSINFO_P2_UPTIME_ROW  19

/* Page 3 rows */
#define UI_SYSINFO_P3_SECT1_ROW   8    /* MEMORY INFORMATION */
#define UI_SYSINFO_P3_RAM_TOT_ROW 11
#define UI_SYSINFO_P3_RAM_FREE_ROW 12
#define UI_SYSINFO_P3_RAM_USED_ROW 13
#define UI_SYSINFO_P3_RAM_PCT_ROW 14
#define UI_SYSINFO_P3_SECT2_ROW   15   /* DISK INFORMATION */
#define UI_SYSINFO_P3_DISK_TOT_ROW 18
#define UI_SYSINFO_P3_DISK_FREE_ROW 19
#define UI_SYSINFO_P3_DISK_USED_ROW 20
#define UI_SYSINFO_P3_DISK_PCT_ROW 21

                                          /* ================================================================
                                             SECTION 15 - CALCULATOR  
                                             ================================================================ */

#define UI_CALC_TITLE_ROW         5
#define UI_CALC_TITLE_COL         19
#define UI_CALC_SUPPORT_ROW       6
#define UI_CALC_SUPPORT_COL       19
#define UI_CALC_HINT_ROW          7
#define UI_CALC_HINT_COL          19
#define UI_CALC_INPUT_ROW         9
#define UI_CALC_INPUT_COL         19
#define UI_CALC_RESULT_ROW        11
#define UI_CALC_RESULT_COL        19

                                             /* ================================================================
                                                SECTION 16 - NOTEPAD  (full screen)
                                                ================================================================ */

#define NP_ROW0                   2    /* ribbon row */
#define NP_COL0                   1
#define NP_WIN_COLS              78    /* 80 - 2 border cells */
#define NP_WIN_ROWS              23    /* 25 - 2 border rows  */
#define NP_EDIT_ROWS             (NP_WIN_ROWS - 2)   /* = 21 */
#define NP_STATUS_ROW            23

                                                /* ================================================================
                                                   SECTION 17 - HELP SCREENS
                                                   All three help screens share the same layout.
                                                   ================================================================ */

#define UI_HELP_TITLE_ROW         7    /* beside logo */
#define UI_HELP_TITLE_COL        19
#define UI_HELP_SEP_ROW           6
#define UI_HELP_SEP_COL          19
#define UI_HELP_BODY_ROW          7    /* items start beside logo */
#define UI_HELP_BODY_COL         19
/* Longest cmd label in prompt_help.c ("  quad_eqn_solve  " etc.) is
   18 visible chars starting at UI_HELP_BODY_COL=19, ending col 36.
   DESC_COL must start at col 37+ or the description overwrites the
   tail of the command label. */
#define UI_HELP_DESC_COL         37

                                                   /* ================================================================
                                                      SECTION 18 - IMPROVEMENTS SCREEN
                                                      ================================================================ */

#define UI_IMPROVE_TITLE_ROW      5    /* beside logo */
#define UI_IMPROVE_TITLE_COL     19
#define UI_IMPROVE_BODY_ROW       6    /* items beside logo */
#define UI_IMPROVE_BODY_COL      19

                                                      /* ================================================================
                                                         SECTION 19 - USER MANAGEMENT WIZARDS
                                                         add_user, userid_change, password_change all share this base.
                                                         ================================================================ */

#define UI_WIZARD_TITLE_ROW       5    /* beside logo */
#define UI_WIZARD_TITLE_COL      19
#define UI_WIZARD_INST_ROW        6
#define UI_WIZARD_INST_COL       19
#define UI_WIZARD_INPUT_ROW       8    /* first input field */
#define UI_WIZARD_INPUT_COL      19

                                                         /* ================================================================
                                                            SECTION 20 - APP PLACEHOLDERS
                                                            ================================================================ */

#define UI_PLACEHOLDER_ROW        8
#define UI_PLACEHOLDER_COL       19

                                                            /* ================================================================
                                                               SECTION 21 - DERIVED HELPERS
                                                               ================================================================ */

#define UI_CENTER_COL            40
#define UI_CONTENT_LOGO_WIDTH    (UI_COL_RIGHT - UI_COL_LOGO + 1)    /* 61 cols */
#define UI_CONTENT_FULL_WIDTH    (UI_COL_RIGHT - UI_COL_NO_LOGO + 1) /* 78 cols */
#define UI_CENTER_OF(len)        ((UI_COLS - (len)) / 2)
/* ================================================================
 SECTION 22 - QUICK REFERENCE TABLE
 Zone              Rows     Cols      Who draws / rule
 -----------------------------------------------------------
 Top border         1       1-80      layout()
 OE name            2       center    layout()
 Build type         3       center    layout()
 Logo               4-10    2-17      logo()
 Logo content       5-9     19-79     modules (beside logo)
 After logo         11-22   19-79     modules (same col 19!)
 Status bar         23      2-79      ui_status() only
 Buffer             24      -         blank
 Bottom border      25      1-80      layout()
 -----------------------------------------------------------
 No-logo screens (calculator, sysinfo, notepad): col 2
 ================================================================ */

 /* ================================================================
    SECTION 23 - DIRECTORY LISTING (ldr command)
    ================================================================ */
#define UI_LDR_HEADER_ROW       4    /* "Directory of: <path>" */
#define UI_LDR_HEADER_COL       UI_COL_LOGO   /* 19 */
#define UI_LDR_COLHEAD_ROW      5    /* "NAME" and "SIZE(kb)" headers */
#define UI_LDR_COLHEAD_COL      UI_COL_LOGO
#define UI_LDR_SEP_ROW          6    /* ---- separator line ---- */
#define UI_LDR_SEP_COL          UI_COL_LOGO
#define UI_LDR_FIRST_ROW        7    /* first file/dir entry */
#define UI_LDR_COL              UI_COL_LOGO
#define UI_LDR_WIDTH            58   /* cols 19-76 */
#define UI_LDR_PAGE_SIZE        15   /* entries per page */

#endif /* UI_COORDINATES_H */