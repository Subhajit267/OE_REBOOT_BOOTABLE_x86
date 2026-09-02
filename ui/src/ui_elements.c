///*
//------------------------------------------------------------
//Author: Subhajit Halder
//Date Created: 2026-02-21
//Date Last Modified: 2026-02-21
//Module: UI Elements
//File: ui_elements.c
//About: Implements layout drawing and theme system
//Revisions:
//- 2026-02-21  Integrated binary theme loader
//- 2026-02-21  Fixed cursor indexing (1-based)
//------------------------------------------------------------
//*/
//
//#include "pal.h"
//#include "file.h"
//#include "ui_elements.h"
//#include "branding.h"
//#define LOOKS_FILE "looks.bd"
//#define DEFAULT_BORDER_COLOR 4   /* Blue */
//
///* ================= THEME ================= */
//
//int ui_get_border_color(void)
//{
//    int color = file_read_int_default(LOOKS_FILE, DEFAULT_BORDER_COLOR);
//
//    if (color < 1 || color > 16)
//        return DEFAULT_BORDER_COLOR;
//
//    return color;
//}
//
//
/////* ================= BORDER ================= */
////
////void border(void)
////{
////    int width =300;
////    int height = 200;
////
////    const char* bg = pal_get_bg(ui_get_border_color());
////
////    /* Top and bottom */
////    for (int i = 1; i <= width; i++)
////    {
////        pal_set_cursor(1, i);
////        pal_print(bg); pal_print(" ");
////
////        pal_set_cursor(height, i);
////        pal_print(bg); pal_print(" ");
////    }
////
////    /* Left and right */
////    for (int i = 1; i <= height; i++)
////    {
////        pal_set_cursor(i, 1);
////        pal_print(bg); pal_print(" ");
////
////        pal_set_cursor(i, width);
////        pal_print(bg); pal_print(" ");
////    }
////
////    pal_print(reset);
////}
//
////void layout()//main looks the floating form design
////{
////    const int WIDTH = 209;      // total columns
////    const int HEIGHT = 50;     // total rows
////    const char* bg = pal_get_bg(ui_get_border_color());
////
////    pal_clear_screen();
////    pal_set_cursor(1, 1);
////
////    // ========== TOP BORDER ==========
////    pal_print("  ");               // two normal spaces outside the border
////    pal_print(bg);                 // background color on
////    for (int i = 0; i < WIDTH - 2; i++) pal_print(" ");  // fill with background spaces
////    pal_print(reset);
////    pal_print("\n");
////
////    // ========== INTERIOR LINES (with optional text) ==========
////    int interior_width = WIDTH - 4;   // space between left and right border cells
////
////    for (int row = 0; row < HEIGHT - 2; row++)   // HEIGHT-2 interior lines
////    {
////        // --- Left border ---
////        pal_print("  ");           // two normal spaces
////        pal_print(bg);             // background on
////        pal_print(" ");            // one background space (left border cell)
////        pal_print(reset);          // back to normal
////
////        // --- Determine if this row has header text ---
////        const char* text = NULL;
////        const char* style = NULL;
////        int text_col = 0;          // 1‑based column where text should start
////
////        if (row == 0)               // first interior line → "Operating Environment"
////        {
////            text = "Operating Environment";
////            style = RED bold underline;
////            text_col = 58;          // as in original (after tabs)
////        }
////        else if (row == 1)          // second interior line → version
////        {
////            text = "Version:5.01.08";
////            style = blue bold underline;
////            text_col = 60;
////        }
////        else if (row == 2)          // third interior line → build type
////        {
////            text = "(Final Release Build C++ Based)";
////            style = yellow bold;
////            text_col = 53;
////        }
////
////        if (text)
////        {
////            // Print spaces up to the text position
////            int spaces_before = text_col - 4;   // because interior starts at column 4
////            for (int i = 0; i < spaces_before; i++) pal_print(" ");
////
////            // Print the text with its style
////            pal_print(style);
////            pal_print(text);
////            pal_print(reset);
////
////            // Fill the rest of the interior with normal spaces
////            int text_len = pal_strlen(text);
////            int spaces_after = interior_width - spaces_before - text_len;
////            for (int i = 0; i < spaces_after; i++) pal_print(" ");
////        }
////        else
////        {
////            // No text → fill entire interior with normal spaces
////            for (int i = 0; i < interior_width; i++) pal_print(" ");
////        }
////
////        // --- Right border ---
////        pal_print(bg);
////        pal_print(" ");
////        pal_print(reset);
////        pal_print("\n");
////    }
////
////    // ========== BOTTOM BORDER ==========
////    pal_print("  ");
////    pal_print(bg);
////    for (int i = 0; i < WIDTH - 2; i++) pal_print(" ");
////    pal_print(reset);
////    pal_print("\n");
////
////    // Return cursor to top (optional, but matches original gotoxy(1,1) at end)
////    pal_set_cursor(1, 1);
////}
////
////void layout(void)
////{
////    border();
////}
//
//void layout()
//{
//    const int WIDTH = UI_COLS;
//    const int HEIGHT = UI_ROWS;
//    const char* bg = pal_get_bg(ui_get_border_color());
//    const int interior_width = WIDTH - 4;   // space between left and right border cells
//
//    char line[500];   // big enough for one line + ANSI codes
//    int pos;
//
//    // ----- TOP BORDER -----
//    pos = 0;
//    line[pos++] = ' '; line[pos++] = ' ';               // two normal spaces
//    pal_strcpy(line + pos, bg); pos += pal_strlen(bg); // background on
//    for (int i = 0; i < WIDTH - 2; i++) line[pos++] = ' '; // fill
//    pal_strcpy(line + pos, reset); pos += pal_strlen(reset);
//    line[pos++] = '\n';
//    line[pos] = '\0';
//    pal_print(line);
//
//    // ----- INTERIOR LINES -----
//    for (int row = 0; row < HEIGHT - 2; row++)
//    {
//        pos = 0;
//
//        // Left border
//        line[pos++] = ' '; line[pos++] = ' ';
//        pal_strcpy(line + pos, bg); pos += pal_strlen(bg);
//        line[pos++] = ' ';
//        pal_strcpy(line + pos, reset); pos += pal_strlen(reset);
//
//        // Determine if this row has header text
//        const char* text = NULL;
//        const char* style = NULL;
//        int text_len = 0;
//
//        if (row == 2)
//        {
//            text = OE_NAME;
//            style = RED bold underline;
//            text_len = pal_strlen(text);
//        }
//        else if (row == 3)
//        {
//            text = "Version:" OE_VERSION;
//            style = blue bold underline;
//            text_len = pal_strlen(text);
//        }
//        else if (row == 4)
//        {
//            text = OE_BUILD_TYPE;
//            style = yellow bold;
//            text_len = pal_strlen(text);
//        }
//
//        if (text)
//        {
//            // Compute centered position inside interior
//            int start_col_in_interior = (interior_width - text_len) / 2;
//            // Ensure non-negative
//            if (start_col_in_interior < 0) start_col_in_interior = 0;
//
//            // Fill spaces up to the text
//            for (int i = 0; i < start_col_in_interior; i++) line[pos++] = ' ';
//
//            // Print styled text
//            pal_strcpy(line + pos, style); pos += pal_strlen(style);
//            pal_strcpy(line + pos, text);  pos += text_len;
//            pal_strcpy(line + pos, reset); pos += pal_strlen(reset);
//
//            // Fill remaining interior with spaces
//            int spaces_after = interior_width - start_col_in_interior - text_len;
//            for (int i = 0; i < spaces_after; i++) line[pos++] = ' ';
//        }
//        else
//        {
//            // No text: fill entire interior with normal spaces
//            for (int i = 0; i < interior_width; i++) line[pos++] = ' ';
//        }
//
//        // Right border
//        pal_strcpy(line + pos, bg); pos += pal_strlen(bg);
//        line[pos++] = ' ';
//        pal_strcpy(line + pos, reset); pos += pal_strlen(reset);
//        line[pos++] = '\n';
//        line[pos] = '\0';
//        pal_print(line);
//    }
//
//    // ----- BOTTOM BORDER -----
//    pos = 0;
//    line[pos++] = ' '; line[pos++] = ' ';
//    pal_strcpy(line + pos, bg); pos += pal_strlen(bg);
//    for (int i = 0; i < WIDTH - 2; i++) line[pos++] = ' ';
//    pal_strcpy(line + pos, reset); pos += pal_strlen(reset);
//    line[pos++] = '\n';
//    line[pos] = '\0';
//    pal_print(line);
//
//    // Return cursor to top (optional)
//    pal_set_cursor(1, 1);
//}
//
/////* ================= LINES ================= */
////
////void hline(int row, int col, int length)
////{
////    const char* bg = pal_get_bg(ui_get_border_color());
////
////    for (int i = 0; i < length; i++)
////    {
////        pal_set_cursor(row, col + i);
////        pal_print(bg); pal_print(" ");
////    }
////
////    pal_print(reset);
////}
////
////void vline(int row, int col, int height)
////{
////    const char* bg = pal_get_bg(ui_get_border_color());
////
////    for (int i = 0; i < height; i++)
////    {
////        pal_set_cursor(row + i, col);
////        pal_print(bg); pal_print(" ");
////    }
////
////    pal_print(reset);
////}
////
////void box(int row, int col, int width, int height)
////{
////	pal_clear_screen();
////    hline(row, col, width);
////    hline(row + height - 1, col, width);
////
////    vline(row, col, height);
////    vline(row, col + width - 1, height);
////}
//
///* ================= LOGO ================= */
//
//void logo(void)
//{
//    int start_row = UI_LOGO_ROW_START;
//    int start_col = UI_LOGO_COL_START;
//
//    /* Top border (34 chars: space + 32 '=' + space) */
//    pal_set_cursor(start_row, start_col);
//    pal_print(RED bold " ================================ " reset);
//
//    /* Line 1: top dashes (blue) */
//    pal_set_cursor(start_row + 1, start_col);
//    pal_print(RED bold "|");
//    pal_print(blue bold "   ------------    -----------  ");
//    pal_print(RED bold "|" reset);
//
//    /* Lines 2–4: vertical pairs (blue) */
//    pal_set_cursor(start_row + 2, start_col);
//    pal_print(RED bold "|");
//    pal_print(blue bold "  |            |  |             ");
//    pal_print(RED bold "|" reset);
//
//    pal_set_cursor(start_row + 3, start_col);
//    pal_print(RED bold "|");
//    pal_print(blue bold "  |            |  |             ");
//    pal_print(RED bold "|" reset);
//
//    pal_set_cursor(start_row + 4, start_col);
//    pal_print(RED bold "|");
//    pal_print(blue bold "  |            |  |             ");
//    pal_print(RED bold "|" reset);
//
//    /* Line 5: middle bar (yellow) */
//    pal_set_cursor(start_row + 5, start_col);
//    pal_print(RED bold "|");
//    pal_print(yellow bold "  |            |  |             ");
//    pal_print(RED bold "|" reset);
//
//    /* Line 5: middle bar (yellow) */
//    pal_set_cursor(start_row + 6, start_col);
//    pal_print(RED bold "|");
//    pal_print(yellow bold "  |            |   ===========  ");
//    pal_print(RED bold "|" reset);
//
//    /* Line 5: middle bar (yellow) */
//    pal_set_cursor(start_row + 7, start_col);
//    pal_print(RED bold "|");
//    pal_print(yellow bold "  |            |  |             ");
//    pal_print(RED bold "|" reset);
//
//    /* Lines 6–9: vertical pairs (green) */
//    pal_set_cursor(start_row + 8, start_col);
//    pal_print(RED bold "|");
//    pal_print(green bold "  |            |  |             ");
//    pal_print(RED bold "|" reset);
//
//    pal_set_cursor(start_row + 9, start_col);
//    pal_print(RED bold "|");
//    pal_print(green bold "  |            |  |             ");
//    pal_print(RED bold "|" reset);
//
//    pal_set_cursor(start_row + 10, start_col);
//    pal_print(RED bold "|");
//    pal_print(green bold "  |            |  |             ");
//    pal_print(RED bold "|" reset);
//
//    pal_set_cursor(start_row + 11, start_col);
//    pal_print(RED bold "|");
//    pal_print(green bold "  |            |  |             ");
//    pal_print(RED bold "|" reset);
//
//    /* Line 10: bottom dashes (green) */
//    pal_set_cursor(start_row + 12, start_col);
//    pal_print(RED bold "|");
//    pal_print(green bold "   ------------    -----------  ");
//    pal_print(RED bold "|" reset);
//
//    /* Bottom border */
//    pal_set_cursor(start_row + 13, start_col);
//    pal_print(RED bold " ================================ " reset);
//}
///* ================= PROGRESS BAR ================= */
//
////void progressbar(int row, int col, int width)
////{
////    pal_set_cursor(row, col);
////    pal_print("[");
////
////    for (int i = 0; i < width; i++)
////        pal_print(" ");
////
////    pal_print("]");
////
////    for (int i = 0; i <= width; i++)
////    {
////        pal_set_cursor(row, col + 1 + i);
////        pal_print(bold "=" reset);
////        pal_sleep(0.05);
////    }
////}
//
//void progressbar(int row, int col, int width)
//{
//    // Seed random once (using a simple time‑based seed – we could also use a fixed seed)
//    static int seeded = 0;
//    if (!seeded) {
//        /* Use a simple seed – e.g., based on a counter or a fixed value.
//           For true randomness we could add a pal_time() function, but we'll keep it simple. */
//        pal_srand(12345);  /* or use a value from somewhere, e.g., the current second? */
//        seeded = 1;
//    }
//
//    // Draw empty bar
//    pal_set_cursor(row, col);
//    pal_print(green bold"[" reset);
//    for (int i = 0; i < width; i++)
//        pal_print(" ");
//    pal_print(green bold "]" reset);
//
//    // Step increments and delay ranges
//    struct {
//        int increment;      // percent to add
//        double delay_min;
//        double delay_max;
//    } steps[] = {
//        {20, 0.5, 1.0},   // 0 → 20, pause
//        {15, 0.3, 0.7},   // 20 → 35
//        {32, 0.2, 0.4},   // 35 → 67 (sudden)
//        {23, 0.8, 1.2},   // 67 → 90 (stuck)
//        {10, 0.2, 0.3}    // 90 → 100
//    };
//    int num_steps = sizeof(steps) / sizeof(steps[0]);
//
//    int progress = 0;
//    for (int s = 0; s < num_steps; s++) {
//        progress += steps[s].increment;
//        if (progress > 100) progress = 100;
//
//        int fill = (progress * width) / 100;
//
//        // Draw filled portion up to current fill
//        for (int i = 0; i < fill; i++) {
//            pal_set_cursor(row, col + 1 + i);
//            pal_print(bold blue "-" reset);
//        }
//
//        // Random delay for this step
//        double r = (pal_rand() % 1000) / 1000.0;  // random 0.000–0.999
//        double delay = steps[s].delay_min +
//            r * (steps[s].delay_max - steps[s].delay_min);
//        pal_sleep(delay);
//    }
//
//    // Brief extra pause at full bar
//    pal_sleep(0.2);
//}
/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-03-16
Module: UI Elements
File: ui_elements.c
About: Implements layout drawing and theme system.
       Rewritten for 80x25 VGA grid.
       layout() draws border + centered header rows.
       logo() draws compact OE logo (16 wide, 7 tall).
       progressbar() unchanged (called with UI_PROGRESS_* constants).

Revisions:
- 2026-02-21  Integrated binary theme loader
- 2026-02-21  Fixed cursor indexing (1-based)
- 2026-03-16  Rewritten for 80x25, ui_coordinates.h
- 2026-08-26  BUG FIX: layout() stamped a stale "STATUS: N/A " label
              at row 23, col 4 — row 23 is reserved for ui_status(),
              whose own clear-line starts at col 12 and never reached
              it, so every real status message stayed permanently
              prefixed with "STATUS: ". Removed.
- 2026-08-26  BUG FIX: logo()'s top border was 1 column narrower than
              its bottom border and side rows (13 vs 14 visible
              chars), misaligning the box's top-right corner.
------------------------------------------------------------
*/

#include "pal.h"
#include "file.h"
#include "ui_elements.h"
#include "branding.h"

#define LOOKS_FILE          "looks.bd"
#define DEFAULT_BORDER_COLOR 4   /* Blue */

/* ================= THEME ================= */

int ui_get_border_color(void)
{
    int color = file_read_int_default(LOOKS_FILE, DEFAULT_BORDER_COLOR);
    if (color < 1 || color > 16)
        return DEFAULT_BORDER_COLOR;
    return color;
}

/* ================= LAYOUT ================= */

/*
  Draws the full 80x25 screen frame.

  Row 1   : top border (full width, background color)
  Row 2   : "Operating Environment  ver: X.X.X"  — centered
  Row 3   : "(Pre Release Build C and PAL Based)" — centered
  Rows 4-24: interior blank rows
  Row 25  : bottom border (full width, background color)

  After drawing, cursor is left at row 1 col 1.
  Modules write their own content into rows 4-22 after ui_init() calls this.
*/
void layout(void)
{
    const int WIDTH = UI_COLS;    /* 80 */
    const int HEIGHT = UI_ROWS;    /* 25 */

    const char* bg = pal_get_bg(ui_get_border_color());
    const int   interior_width = WIDTH - 4;   /* cols 3 to WIDTH-2 */

    /* OE name + version string and build type for centered printing */
    const char* header1 = OE_NAME "  ver: " OE_VERSION;
    const char* header2 = OE_BUILD_TYPE;

    char line[512];
    int  pos;
    int  row;

    /* ----- TOP BORDER (row 1) ----- */
    pos = 0;
    line[pos++] = ' '; line[pos++] = ' ';
    pal_strcpy(line + pos, bg);    pos += pal_strlen(bg);
    for (int i = 0; i < WIDTH - 2; i++) line[pos++] = ' ';
    pal_strcpy(line + pos, reset); pos += pal_strlen(reset);
    line[pos++] = '\n'; line[pos] = '\0';
    pal_print(line);

    /* ----- INTERIOR ROWS (rows 2 to HEIGHT-1) ----- */
    for (row = 0; row < HEIGHT - 2; row++)
    {
        pos = 0;

        /* Left border cell */
        line[pos++] = ' '; line[pos++] = ' ';
        pal_strcpy(line + pos, bg);    pos += pal_strlen(bg);
        line[pos++] = ' ';
        pal_strcpy(line + pos, reset); pos += pal_strlen(reset);

        /* Select header text for rows 0 and 1 (= screen rows 2 and 3) */
        const char* text = (char*)0;
        const char* style = (char*)0;
        int text_len = 0;

        if (row == 0)   /* screen row 2 — OE name + version */
        {
            text = header1;
            style = RED bold underline;
            text_len = (int)pal_strlen(text);
        }
        else if (row == 1)   /* screen row 3 — build type */
        {
            text = header2;
            style = yellow bold;
            text_len = (int)pal_strlen(text);
        }

        if (text)
        {
            /* Center text inside interior width */
            int start = (interior_width - text_len) / 2;
            if (start < 0) start = 0;

            for (int i = 0; i < start; i++) line[pos++] = ' ';

            pal_strcpy(line + pos, style); pos += pal_strlen(style);
            pal_strcpy(line + pos, text);  pos += text_len;
            pal_strcpy(line + pos, reset); pos += pal_strlen(reset);

            int trail = interior_width - start - text_len;
            if (trail < 0) trail = 0;
            for (int i = 0; i < trail; i++) line[pos++] = ' ';
        }
        else
        {
            for (int i = 0; i < interior_width; i++) line[pos++] = ' ';
        }

        /* Right border cell */
        pal_strcpy(line + pos, bg);    pos += pal_strlen(bg);
        line[pos++] = ' ';
        pal_strcpy(line + pos, reset); pos += pal_strlen(reset);
        line[pos++] = '\n'; line[pos] = '\0';
        pal_print(line);

    }

    /* ----- BOTTOM BORDER (row HEIGHT) ----- */
    pos = 0;
    line[pos++] = ' '; line[pos++] = ' ';
    pal_strcpy(line + pos, bg);    pos += pal_strlen(bg);
    for (int i = 0; i < WIDTH - 2; i++) line[pos++] = ' ';
    pal_strcpy(line + pos, reset); pos += pal_strlen(reset);
    line[pos++] = '\n'; line[pos] = '\0';
    pal_print(line);

    /* Row 23 is reserved exclusively for ui_status() (see ui_coordinates.h
       SECTION 6). layout() used to stamp a stale "STATUS: N/A " label at
       col 4 here, which ui_status()'s own clear-line (starting at col 12,
       UI_STATUS_COL) never reached — every real status message ended up
       permanently prefixed with a leftover "STATUS: " on screen. Removed;
       layout() must leave row 23 untouched. */
    pal_set_cursor(1, 1);
}

/* ================= LOGO ================= */

/*
  Compact OE logo — 16 chars wide, 7 rows tall.
  Occupies rows UI_LOGO_ROW_START to UI_LOGO_ROW_END (rows 4-10),
  cols UI_LOGO_COL_START to UI_LOGO_COL_END (cols 2-17).

  Visual:
   ================
  | ------  -----  |    row 4 = outer box top
  ||      ||      |    row 5 = O left + E top
  ||      | ===   |    row 6 = O mid  + E middle bar
  ||      ||      |    row 7 = O mid  + E bottom
  | ------  -----  |    row 8 = O bot  + E bot
   ================     row 9 = outer box bot ... wait only 7 rows

  Rows 4-10 = 7 rows:
  Row 4:  " ============== "   outer top  (RED)
  Row 5:  "| ----  -----  |"   O-top + E-top  (blue)
  Row 6:  "||     ||      |"   O-side + E-side (blue)
  Row 7:  "||     | ===   |"   O-mid  + E-bar  (yellow)
  Row 8:  "||     ||      |"   O-side + E-side (yellow)
  Row 9:  "| ----  -----  |"   O-bot  + E-bot  (green)
  Row 10: " ============== "   outer bot  (RED)

  All chars are original OE chars: - = | space only.
*/
void logo(void)
{
    int r = UI_LOGO_ROW_START;
    int c = UI_LOGO_COL_START;

    /* Row 4 — outer top border */
    pal_set_cursor(r++, c);
    pal_print(RED bold " ============ " reset);

    /* Row 5 — O top dash + E top dash (blue) */
    pal_set_cursor(r, c);
    pal_print(RED bold "|");
    pal_print(blue bold " ----  ---- ");
    pal_print(RED bold "|" reset);
    r++;

    /* Row 6 — O side + E side (blue) */
    pal_set_cursor(r, c);
    pal_print(RED bold "|");
    pal_print(blue bold "|    ||     ");
    pal_print(RED bold "|" reset);
    r++;

    /* Row 7 — O mid + E middle bar (yellow) */
    pal_set_cursor(r, c);
    pal_print(RED bold "|");
    pal_print(yellow bold "|    | ===  ");
    pal_print(RED bold "|" reset);
    r++;

    /* Row 8 — O side + E side (yellow) */
    pal_set_cursor(r, c);
    pal_print(RED bold "|");
    pal_print(yellow bold "|    ||     ");
    pal_print(RED bold "|" reset);
    r++;

    /* Row 9 — O bot dash + E bot dash (green) */
    pal_set_cursor(r, c);
    pal_print(RED bold "|");
    pal_print(green bold " ----  ---- ");
    pal_print(RED bold "|" reset);
    r++;

    /* Row 10 — outer bottom border */
    pal_set_cursor(r, c);
    pal_print(RED bold " ============ " reset);
}

/* ================= PROGRESS BAR ================= */

void progressbar(int row, int col, int width)
{
    static int seeded = 0;
    if (!seeded)
    {
        pal_srand(12345);
        seeded = 1;
    }

    /* Draw empty bar */
    pal_set_cursor(row, col);
    pal_print(green bold "[" reset);
    for (int i = 0; i < width; i++) pal_print(" ");
    pal_print(green bold "]" reset);

    /* Step increments and delay ranges */
    struct { int inc; double dmin; double dmax; } steps[] = {
        { 20, 0.5, 1.0 },
        { 15, 0.3, 0.7 },
        { 32, 0.2, 0.4 },
        { 23, 0.8, 1.2 },
        { 10, 0.2, 0.3 }
    };
    int num_steps = (int)(sizeof(steps) / sizeof(steps[0]));
    int progress = 0;

    for (int s = 0; s < num_steps; s++)
    {
        progress += steps[s].inc;
        if (progress > 100) progress = 100;

        int fill = (progress * width) / 100;

        for (int i = 0; i < fill; i++)
        {
            pal_set_cursor(row, col + 1 + i);
            pal_print(bold blue "-" reset);
        }

        double rnd = (double)(pal_rand() % 1000) / 1000.0;
        double delay = steps[s].dmin + rnd * (steps[s].dmax - steps[s].dmin);
        pal_sleep(delay);
    }

    pal_sleep(0.2);
}
