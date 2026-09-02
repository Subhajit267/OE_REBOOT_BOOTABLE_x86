///*
//------------------------------------------------------------
//Author: Subhajit Halder
//Date Created: 2026-02-21
//Date Last Modified: 2026-03-10
//Module: Help Documentation
//File: prompt_help.c
//About: Displays help information for main prompt
//Revisions:
//- 2026-02-21  Converted legacy gotoxy help to PAL-based layout
//- 2026-03-05  Added filesystem command documentation
//- 2026-03-05  Improved layout and color scheme
//- 2026-03-10  Added notepad command documentation
//------------------------------------------------------------
//*/
//
//#include "pal.h"
//#include "help.h"
//
//void help_show_prompt(void)
//{
//	int row = 13, column = 78;
//	pal_set_cursor(row++, column);
//	pal_print(green bold "Welcome to PROMPT HELP Documentation Ver: 2.1" reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(yellow bold "--------------------------------------------" reset);
//
//
//	/* -------- INSTALLABLE APPLICATIONS -------- */
//
//	pal_set_cursor(row++, column);
//	pal_print(yellow bold "Installable Applications:" reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tTicTacToe        " purple ": Two player or computer TicTacToe game." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tQuiz             " purple ": Multiround quiz system with scoreboard." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tTemp_Conv        " purple ": Convert Celsius, Fahrenheit and Kelvin." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\teqn_solve        " purple ": Solve linear equations in two variables." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tquad_eqn_solve   " purple ": Solve quadratic equations." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tatc              " purple ": Area of Triangle calculator." reset);
//
//
//	/* -------- SYSTEM COMMANDS -------- */
//
//	row++;
//
//	pal_set_cursor(row++, column);
//	pal_print(yellow bold "System Commands:" reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\texit             " purple ": Exit the OE environment." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tver              " purple ": Display OE version." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tlogin            " purple ": Re-login to OE system." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tsettings         " purple ": Modify OE settings." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tregistryeditor   " purple ": Enter registry editor mode." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tsysteminfo       " purple ": Display system hardware information." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tcalculator       " purple ": Evaluate mathematical expressions." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tnotepad [file]   " purple ": Open text editor. Optionally pass a filename." reset);
//
//
//	/* -------- APPLICATION MANAGEMENT -------- */
//
//	row++;
//
//	pal_set_cursor(row++, column);
//	pal_print(yellow bold "Application Management:" reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tinst_all         " purple ": Install all available applications." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tunst_all         " purple ": Uninstall all installed applications." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tinst_<appname>   " purple ": Install specific application." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tunst_<appname>   " purple ": Uninstall specific application." reset);
//
//
//	/* -------- FILESYSTEM COMMANDS -------- */
//
//	row++;
//
//	pal_set_cursor(row++, column);
//	pal_print(yellow bold "Filesystem Commands:" reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tmdr <n>          " purple ": Create directory." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\trdr <n>          " purple ": Remove directory." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tldr              " purple ": List directory contents." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tcdr <dir>        " purple ": Change directory." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tcdr ..           " purple ": Move to parent directory." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tpwd              " purple ": Show current working directory." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\trmf <file>       " purple ": Remove file." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tcpf <src> <dst>  " purple ": Copy file." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\trdf <file>       " purple ": Read file contents." reset);
//
//	pal_set_cursor(row++, column);
//	pal_print(cyan bold "\tmvf <src> <dst>  " purple ": Move file." reset);
//
//	pal_pause();
//
//}
/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-03-16
Module: Help Documentation
File: prompt_help.c
About: Displays help information for main prompt.
       Rewritten for 80x25 grid with pagination (3 pages).
       Page 1: Installable Applications + System Commands (partial)
       Page 2: System Commands (rest) + App Management
       Page 3: Filesystem Commands

Revisions:
- 2026-02-21  Converted legacy gotoxy help to PAL-based layout
- 2026-03-05  Added filesystem command documentation
- 2026-03-10  Added notepad command documentation
- 2026-03-16  Rewritten for 80x25, paginated, ui_coordinates.h
- 2026-08-25  BUG FIX: help_page_status() ignored its "page"/"total"
              params (ternaries always printed "3"), and the unused
              "buf" local was cast to void instead of being used.
              Now formats both numbers with pal_itoa() so the
              function actually works for any page/total.
- 2026-08-26  BUG FIX: UI_HELP_DESC_COL was 34, only 15 cols from
              UI_HELP_BODY_COL=19 — several 14-15 char cmd labels
              (quad_eqn_solve, registryeditor, notepad [file],
              inst_<appname>, unst_<appname>, cpf/mvf <src> <dst>)
              ran past col 34 and had their tail overwritten by the
              description text. Fixed by widening
              UI_HELP_DESC_COL to 37 (ui_coordinates.h) and
              shortening the one description that would then have
              overflowed column 79 at the new offset.
------------------------------------------------------------
*/

#include "pal.h"
#include "ui_setup.h"
#include "ui_elements.h"
#include "help.h"

/* Print one help entry: "  cmd          : description" */
static void help_entry(int* row, const char* cmd, const char* desc)
{
    ui_title(*row, UI_HELP_BODY_COL, cyan bold, cmd);
    ui_title(*row, UI_HELP_DESC_COL, purple, desc);
    (*row)++;
}

/* Print a section header */
static void help_section(int* row, const char* title)
{
    (*row)++;
    ui_title(*row, UI_HELP_BODY_COL, yellow bold, title);
    (*row)++;
}

/* Print the page indicator in status bar */
static void help_page_status(int page, int total)
{
    char buf[16];
    pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
    pal_print(yellow bold "[Page ");
    pal_itoa(page, buf);
    pal_print(buf);
    pal_print("/");
    pal_itoa(total, buf);
    pal_print(buf);
    if (page < total)
        pal_print(" - Press any key for next]" reset);
    else
        pal_print(" - Press any key to return]" reset);
}

void help_show_prompt(void)
{
    int row;

    /* ============================================================
       PAGE 1 — Title + Installable Apps + System Commands (partial)
       ============================================================ */
    ui_init();
    row = UI_HELP_TITLE_ROW;

    ui_title(row++, UI_HELP_TITLE_COL, green bold,
        "Welcome to PROMPT HELP Documentation Ver: 2.1");
    ui_title(row++, UI_HELP_SEP_COL, "", "--------------------------------------------");

    help_section(&row, "Installable Applications:");
    help_entry(&row, "  TicTacToe       ", ": Two player or computer TicTacToe game.");
    help_entry(&row, "  Quiz            ", ": Multiround quiz system with scoreboard.");
    help_entry(&row, "  Temp_Conv       ", ": Convert Celsius, Fahrenheit and Kelvin.");
    help_entry(&row, "  eqn_solve       ", ": Solve linear equations in two variables.");
    help_entry(&row, "  quad_eqn_solve  ", ": Solve quadratic equations.");
    help_entry(&row, "  atc             ", ": Area of Triangle calculator.");

    help_section(&row, "System Commands:");
    help_entry(&row, "  exit            ", ": Exit the OE environment.");
    help_entry(&row, "  ver             ", ": Display OE version.");
    help_entry(&row, "  login           ", ": Re-login to OE system.");
    help_entry(&row, "  settings        ", ": Modify OE settings.");

    help_page_status(1, 4);
    pal_pause();

    /* ============================================================
       PAGE 2 — System Commands (rest) + App Management
       ============================================================ */
    ui_init();
    row = UI_HELP_TITLE_ROW;

    ui_title(row++, UI_HELP_TITLE_COL, green bold,
        "PROMPT HELP - Page 2/4");
    ui_title(row++, UI_HELP_SEP_COL, "", "--------------------------------------------");

    help_section(&row, "System Commands (continued):");
    help_entry(&row, "  registryeditor  ", ": Enter registry editor mode.");
    help_entry(&row, "  systeminfo      ", ": Display system hardware information.");
    help_entry(&row, "  calculator      ", ": Evaluate mathematical expressions.");
    help_entry(&row, "  notepad [file]  ", ": Open text editor. Optional filename.");

    help_section(&row, "Application Management:");
    help_entry(&row, "  inst_all        ", ": Install all available applications.");
    help_entry(&row, "  unst_all        ", ": Uninstall all installed applications.");
    help_entry(&row, "  inst_<appname>  ", ": Install a specific application.");
    help_entry(&row, "  unst_<appname>  ", ": Uninstall a specific application.");

    help_page_status(2, 4);
    pal_pause();

    /* ============================================================
       PAGE 3 — Filesystem Commands
       ============================================================ */
    ui_init();
    row = UI_HELP_TITLE_ROW;

    ui_title(row++, UI_HELP_TITLE_COL, green bold,
        "PROMPT HELP - Page 3/4");
    ui_title(row++, UI_HELP_SEP_COL, "", "--------------------------------------------");

    help_section(&row, "Filesystem Commands:");
    help_entry(&row, "  mdr <n>         ", ": Create directory.");
    help_entry(&row, "  rdr <n>         ", ": Remove directory.");
    help_entry(&row, "  ldr             ", ": List directory contents.");
    help_entry(&row, "  cdr <dir>       ", ": Change directory.");
    help_entry(&row, "  cdr ..          ", ": Move to parent directory.");
    help_entry(&row, "  pwd             ", ": Show current working directory.");
    help_entry(&row, "  rmf <file>      ", ": Remove file.");
    help_entry(&row, "  cpf <src> <dst> ", ": Copy file.");
    help_entry(&row, "  rdf <file>      ", ": Read file contents.");
    help_entry(&row, "  mvf <src> <dst> ", ": Move file.");

    help_page_status(3, 4);
    pal_pause();

    /* ============================================================
       PAGE 4 — Directory Copy/Move/Rename (kept off page 3 -- that
       page's 10 entries already fill rows 11-20 with UI_STATUS_ROW
       at 23, no room left for 3 more without colliding with the
       page-status line)
       ============================================================ */
    ui_init();
    row = UI_HELP_TITLE_ROW;

    ui_title(row++, UI_HELP_TITLE_COL, green bold,
        "PROMPT HELP - Page 4/4");
    ui_title(row++, UI_HELP_SEP_COL, "", "--------------------------------------------");

    help_section(&row, "Directory Copy/Move/Rename:");
    help_entry(&row, "  cpdr <src> <dst>", ": Copy directory (top-level files only).");
    help_entry(&row, "  mvdr <src> <dst>", ": Move directory.");
    /* "  rnmdr <old> <new>" is 19 visible chars -- 1 over the 18-char
       budget this help screen enforces before UI_HELP_DESC_COL (see the
       2026-08-26 overflow fix above); trimmed to one leading space. */
    help_entry(&row, " rnmdr <old> <new>", ": Rename directory.");

    help_page_status(4, 4);
    pal_pause();
}