/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-31
Module: HAL
File: vga.c
About: VGA text-mode console with a minimal ANSI/CSI escape-sequence
       interpreter -- see the file's own detailed header comment below
       for exactly which sequences are supported and why this driver
       has to BE a terminal, not just a text writer. Also owns the
       hardware cursor (position, shape, blink-disable via the
       Attribute Controller).
Revisions:
- 2026-08-30  Initial creation (Phase 1): plain scrolling text writer.
- 2026-08-30  Grown into a full ANSI/CSI interpreter (Phase 1 extended)
              once the existing hosted UI's raw escape codes needed a
              real terminal underneath them.
- 2026-08-30  Two latent bugs found once real ANSI-colored UI first
              exercised the driver (Phase 7): ANSI-to-VGA color mapping
              (wrong hardware palette order) and missing deferred/lazy
              line-wrap (double row-advance on an exactly-80-column line
              followed by '\n') -- both fixed, see ansi_to_vga[] and
              vga_draw_char()'s comments.
- 2026-08-31  Added vga_set_cursor_shape(), called explicitly at init
              instead of trusting whatever cursor scanline range the
              platform's own VGA BIOS happened to leave -- fixes a
              cursor-not-tracking report specific to VMware's BIOS.
------------------------------------------------------------
*/

#include "vga.h"
#include "io_ports.h"

/*
   VGA text-mode console with a minimal ANSI/CSI escape-sequence
   interpreter. The existing hosted userland (pal.h's color macros,
   pal_set_cursor, pal_clear_screen, pal_hide/show_cursor, the DECAWM
   auto-wrap toggle in pal_linux.c) prints raw ANSI escapes directly
   into strings and relies on the terminal to interpret them. On real
   hardware there is no terminal underneath us — this driver has to
   BE that terminal, or every colored/positioned screen in the project
   would render as literal garbage like "^[[31m" once ported.

   Supported (this is the exact set found in use across the codebase,
   plus cursor movement A/B/C/D which isn't used yet but is standard
   and cheap to add now):
     SGR   \033[<n>(;<n>)*m   colors, bold, invisible, reset
     CUP   \033[<row>;<col>H  absolute cursor position
     ED    \033[2J            clear screen
     CUU/D/F/B \033[<n>A/B/C/D  relative cursor movement
     DECTCEM \033[?25l \033[?25h  hide/show hardware cursor
     DECAWM  \033[?7l  \033[?7h   disable/enable line auto-wrap

   Anything else (unrecognized SGR codes, unsupported final bytes) is
   safely dropped instead of corrupting state or drawing garbage.
*/

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile unsigned short*)0xB8000)
#define VGA_MAX_PARAMS 4

enum { CSI_NORMAL, CSI_ESC, CSI_BRACKET };

static int vga_row;
static int vga_col;

static int csi_state;
static int csi_params[VGA_MAX_PARAMS];
static int csi_nparams;
static int csi_private;

static int cur_fg;
static int cur_bg;
static int cur_bold;
static int cur_invisible;
static int autowrap;
static int wrap_pending;

/* ============== VGA hardware register access ============== */

static void vga_disable_blink(void)
{
    unsigned char val;

    /* Attribute Controller ports (0x3C0/0x3C1) use an internal index/data
       flip-flop that must be reset by reading the input status register
       before every index write, or the write lands on the wrong half. */
    (void)inb(0x3DA);
    outb(0x3C0, 0x10);          /* select Mode Control register (index 0x10) */
    val = inb(0x3C1);
    val = (unsigned char)(val & ~0x08); /* clear blink-enable */

    (void)inb(0x3DA);
    outb(0x3C0, 0x10);
    outb(0x3C0, val);
    outb(0x3C0, 0x20);          /* re-assert PAS bit: re-enable normal display output */
}

static void vga_update_hw_cursor(void)
{
    unsigned short pos = (unsigned short)(vga_row * VGA_WIDTH + vga_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

static void vga_set_cursor_visible(int visible)
{
    unsigned char cur;
    outb(0x3D4, 0x0A);
    cur = inb(0x3D5);
    if (visible)
        outb(0x3D5, (unsigned char)(cur & ~0x20));
    else
        outb(0x3D5, (unsigned char)(cur | 0x20));
}

/*
   Cursor start/end (CRTC 0x0A/0x0B) set which scanlines of the 16-line
   character cell the cursor block occupies -- this driver used to only
   ever twiddle the show/hide bit on top of whatever scanline range the
   platform's BIOS happened to leave there. That's an inherited, unverified
   value: it looks right in QEMU only because QEMU's default VGA BIOS
   happens to leave a sane underline-cursor range. A different BIOS/VGA
   implementation (this was written chasing a report of the cursor
   rendering wrong under VMware) could leave anything. Setting it
   explicitly removes that dependency. Standard underline-cursor values
   for a 16-scanline cell: start at line 13, end at line 14.
*/
static void vga_set_cursor_shape(unsigned char start_line, unsigned char end_line)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, start_line & 0x1F);
    outb(0x3D4, 0x0B);
    outb(0x3D5, end_line & 0x1F);
}

/* ============== attribute / drawing helpers ============== */

static unsigned char current_attr(void)
{
    int fg = cur_invisible ? cur_bg : cur_fg;
    return (unsigned char)(((cur_bg & 0x0F) << 4) | (fg & 0x0F));
}

static void vga_clear(unsigned char attr)
{
    int i;
    unsigned short fill = (unsigned short)(' ' | (attr << 8));
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = fill;
}

static void vga_scroll(void)
{
    int r, c;
    unsigned short blank = (unsigned short)(' ' | (current_attr() << 8));

    for (r = 0; r < VGA_HEIGHT - 1; r++)
        for (c = 0; c < VGA_WIDTH; c++)
            VGA_MEMORY[r * VGA_WIDTH + c] = VGA_MEMORY[(r + 1) * VGA_WIDTH + c];

    for (c = 0; c < VGA_WIDTH; c++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + c] = blank;

    vga_row = VGA_HEIGHT - 1;
}

static void vga_set_cursor(int row, int col)
{
    if (row < 0) row = 0;
    if (row > VGA_HEIGHT - 1) row = VGA_HEIGHT - 1;
    if (col < 0) col = 0;
    if (col > VGA_WIDTH - 1) col = VGA_WIDTH - 1;

    vga_row = row;
    vga_col = col;
    wrap_pending = 0; /* an explicit cursor move cancels any pending line wrap */
    vga_update_hw_cursor();
}

/*
   Writing the very last column doesn't advance the row immediately --
   it sets wrap_pending and *defers* the advance until the next character
   arrives (standard VT100/xterm "delayed wrap"). Without this, a caller
   that prints a full-width line followed by an explicit '\n' (layout()'s
   80-char border/content rows, in particular) gets TWO row advances for
   one logical line: the auto-wrap from column 80, then the '\n' itself.
   That silently ate every other row, turning a 25-row screen's worth of
   drawing into a striped mess only every 2nd row of which showed real
   content. If the character that resolves the pending wrap is itself a
   '\n', the two coalesce into the single advance a real terminal would
   show, instead of double-advancing.
*/
static void vga_draw_char(char c)
{
    if (wrap_pending && c != '\n')
    {
        wrap_pending = 0;
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT)
            vga_scroll();
    }

    if (c == '\n')
    {
        wrap_pending = 0;
        vga_col = 0;
        vga_row++;
    }
    else if (c == '\b')
    {
        /* Matches a real terminal's plain-backspace behavior (move left,
           don't erase) -- pal_kernel.c's pal_readline pairs this with an
           explicit " \b" to actually blank the erased column. */
        if (vga_col > 0)
            vga_col--;
    }
    else
    {
        VGA_MEMORY[vga_row * VGA_WIDTH + vga_col] =
            (unsigned short)((unsigned char)c | (current_attr() << 8));
        vga_col++;

        if (vga_col >= VGA_WIDTH)
        {
            vga_col = VGA_WIDTH - 1; /* hold at last column either way */
            if (autowrap)
                wrap_pending = 1; /* advance deferred to the next char, see above */
        }
    }

    if (vga_row >= VGA_HEIGHT)
        vga_scroll();

    vga_update_hw_cursor();
}

/* ============== CSI (escape sequence) dispatch ============== */

static int csi_param(int index, int fallback)
{
    if (index >= csi_nparams || csi_params[index] <= 0)
        return fallback;
    return csi_params[index];
}

/*
   ANSI SGR color numbers (0=black,1=red,2=green,3=yellow,4=blue,5=magenta,
   6=cyan,7=white -- the xterm/ANSI ordering pal.h's color macros assume)
   do NOT match the VGA/CGA hardware attribute nibble's own palette order
   (0=black,1=blue,2=green,3=cyan,4=red,5=magenta,6=brown,7=light-gray).
   Writing the ANSI index straight into the attribute byte -- what this
   function used to do -- silently swaps red and blue (and yellow/cyan):
   "\033[44m" (ANSI blue bg) rendered as hardware red. Same table works for
   the bright 90-97/100-107 range too, since VGA's bright half (8-15) keeps
   the same relative ordering as its dim half.
*/
static const unsigned char ansi_to_vga[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

static void vga_csi_dispatch(char final)
{
    int i, n;

    switch (final)
    {
    case 'm':
        if (csi_nparams == 0)
        {
            cur_fg = 15; cur_bg = 0; cur_bold = 0; cur_invisible = 0;
            break;
        }
        for (i = 0; i < csi_nparams; i++)
        {
            int p = csi_params[i];

            if (p == 0)      { cur_fg = 15; cur_bg = 0; cur_bold = 0; cur_invisible = 0; }
            else if (p == 1) cur_bold = 1;
            else if (p == 22) cur_bold = 0;
            else if (p == 4) { /* underline: no VGA color-mode glyph; consumed, no-op */ }
            else if (p == 8) cur_invisible = 1;
            else if (p == 28) cur_invisible = 0;
            else if (p >= 30 && p <= 37)  cur_fg = ansi_to_vga[p - 30] | (cur_bold ? 0x08 : 0);
            else if (p >= 90 && p <= 97)  { cur_fg = ansi_to_vga[p - 90] | 0x08; cur_bold = 1; }
            else if (p >= 40 && p <= 47)  cur_bg = ansi_to_vga[p - 40];
            else if (p >= 100 && p <= 107) cur_bg = ansi_to_vga[p - 100] | 0x08;
            else if (p == 99) cur_bg = 0; /* pal.h B16: "blank (no color)" */
            /* anything else: unrecognized SGR code, ignore */
        }
        break;

    case 'H':
        vga_set_cursor(csi_param(0, 1) - 1, csi_param(1, 1) - 1);
        break;

    case 'J':
        /* codebase only ever sends "2J" (full clear); treat any J as full clear */
        vga_clear(current_attr());
        vga_set_cursor(0, 0);
        break;

    case 'A': n = csi_param(0, 1); vga_set_cursor(vga_row - n, vga_col); break;
    case 'B': n = csi_param(0, 1); vga_set_cursor(vga_row + n, vga_col); break;
    case 'C': n = csi_param(0, 1); vga_set_cursor(vga_row, vga_col + n); break;
    case 'D': n = csi_param(0, 1); vga_set_cursor(vga_row, vga_col - n); break;

    case 'h':
    case 'l':
        if (csi_private && csi_nparams >= 1)
        {
            if (csi_params[0] == 25) vga_set_cursor_visible(final == 'h');
            else if (csi_params[0] == 7) autowrap = (final == 'h');
        }
        break;

    default:
        break; /* unsupported final byte: drop the sequence safely */
    }
}

/* ============== public API ============== */

void vga_initialize(void)
{
    csi_state = CSI_NORMAL;
    csi_nparams = 0;
    csi_private = 0;

    cur_fg = 15;   /* bright white */
    cur_bg = 0;    /* black */
    cur_bold = 0;
    cur_invisible = 0;
    autowrap = 1;
    wrap_pending = 0;

    vga_disable_blink();
    vga_clear(current_attr());

    vga_row = 0;
    vga_col = 0;
    vga_set_cursor_shape(13, 14);
    vga_set_cursor_visible(1);
    vga_update_hw_cursor();
}

void vga_putchar(char c)
{
    switch (csi_state)
    {
    case CSI_NORMAL:
        if ((unsigned char)c == 0x1B) { csi_state = CSI_ESC; return; }
        vga_draw_char(c);
        return;

    case CSI_ESC:
        if (c == '[')
        {
            csi_state = CSI_BRACKET;
            csi_nparams = 0;
            csi_params[0] = 0;
            csi_private = 0;
        }
        else
        {
            csi_state = CSI_NORMAL; /* unsupported ESC sequence: drop it */
        }
        return;

    case CSI_BRACKET:
        if (c == '?')
        {
            csi_private = 1;
            return;
        }
        if (c >= '0' && c <= '9')
        {
            if (csi_nparams == 0) csi_nparams = 1;
            csi_params[csi_nparams - 1] = csi_params[csi_nparams - 1] * 10 + (c - '0');
            return;
        }
        if (c == ';')
        {
            if (csi_nparams < VGA_MAX_PARAMS)
            {
                csi_params[csi_nparams] = 0;
                csi_nparams++;
            }
            return;
        }
        vga_csi_dispatch(c);
        csi_state = CSI_NORMAL;
        return;
    }
}

void vga_print(const char* str)
{
    while (*str)
        vga_putchar(*str++);
}

void vga_get_size(int* rows, int* cols)
{
    *rows = VGA_HEIGHT;
    *cols = VGA_WIDTH;
}
