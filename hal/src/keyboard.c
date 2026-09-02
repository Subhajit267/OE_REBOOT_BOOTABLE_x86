/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-09-01
Module: HAL
File: keyboard.c
About: PS/2 keyboard driver, IRQ1, scancode set 1 (US QWERTY). Feeds two
       ring buffers from every keystroke: a cooked ASCII queue (shift
       already applied, key releases/unmapped keys dropped) and a raw
       queue (undecoded scancode + 0xE0-prefix flag + best-effort ASCII)
       -- pal_kernel.c's pal_getchar()/pal_readline() drain the cooked
       one, pal_raw_getkey() (notepad's editor) drains the raw one.
Revisions:
- 2026-08-30  Initial creation (Phase 2): cooked queue only, keystrokes
              echoed directly to the console by this driver.
- 2026-08-30  Raw queue added (Phase 6) for real input queueing; echo
              moved out of the driver into pal_kernel.c (cooked mode
              echoes, raw mode doesn't -- matches real terminal
              semantics instead of always echoing at the hardware layer).
- 2026-09-01  Added keyboard_flush_raw()/keyboard_flush_cooked() -- both
              queues fill unconditionally regardless of which mode is
              "active", so a mode switch with no flush let stale
              keystrokes from the previous mode replay into the new one
              (root cause of notepad opening a file and showing the
              just-typed launch command as garbage leading text).
------------------------------------------------------------
*/

#include "keyboard.h"
#include "io_ports.h"
#include "irq.h"

#define KBD_DATA_PORT 0x60
#define SCANCODE_EXTENDED_PREFIX 0xE0

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_LSHIFT_REL (SC_LSHIFT | 0x80)
#define SC_RSHIFT_REL (SC_RSHIFT | 0x80)

/*
   PS/2 scancode set 1, US QWERTY, indexed by make code. 0 marks a key this
   table doesn't turn into a character (function keys, numpad, arrows,
   ctrl/alt/capslock) -- those still reach pal_kernel.c's raw-mode consumer
   via the raw queue below, just not the cooked one.
*/
static const char scancode_ascii[128] = {
    0,   27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0,  ' '
    /* remaining indices default to 0 (unmapped) via static zero-init */
};

static const char scancode_ascii_shift[128] = {
    0,   27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0,  ' '
};

static int shift_held;
static int pending_extended;

#define COOKED_QUEUE_SIZE 256
static char cooked_queue[COOKED_QUEUE_SIZE];
static unsigned int cooked_head, cooked_tail;

#define RAW_QUEUE_SIZE 64
struct raw_event { unsigned char scancode; int extended; char ascii; };
static struct raw_event raw_queue[RAW_QUEUE_SIZE];
static unsigned int raw_head, raw_tail;

static void cooked_push(char c)
{
    unsigned int next = (cooked_tail + 1) % COOKED_QUEUE_SIZE;
    if (next == cooked_head)
        return; /* queue full: drop -- a human typing can't outrun this in practice */
    cooked_queue[cooked_tail] = c;
    cooked_tail = next;
}

static void raw_push(unsigned char scancode, int extended, char ascii)
{
    unsigned int next = (raw_tail + 1) % RAW_QUEUE_SIZE;
    if (next == raw_head)
        return;
    raw_queue[raw_tail].scancode = scancode;
    raw_queue[raw_tail].extended = extended;
    raw_queue[raw_tail].ascii    = ascii;
    raw_tail = next;
}

static void keyboard_handler(void)
{
    unsigned char code = inb(KBD_DATA_PORT);
    int extended = pending_extended;

    if (code == SCANCODE_EXTENDED_PREFIX)
    {
        pending_extended = 1;
        return;
    }
    pending_extended = 0;

    if (code == SC_LSHIFT || code == SC_RSHIFT)      { shift_held = 1; return; }
    if (code == SC_LSHIFT_REL || code == SC_RSHIFT_REL) { shift_held = 0; return; }

    if (code & 0x80)
        return; /* key release: nothing else to react to without fuller key-state tracking */

    {
        char ascii = 0;
        if (!extended && code < 128)
            ascii = shift_held ? scancode_ascii_shift[code] : scancode_ascii[code];

        raw_push(code, extended, ascii);
        if (ascii)
            cooked_push(ascii);
    }
}

void keyboard_initialize(void)
{
    irq_install_handler(1, keyboard_handler);
}

int keyboard_try_read_char(void)
{
    char c;

    if (cooked_head == cooked_tail)
        return -1;

    c = cooked_queue[cooked_head];
    cooked_head = (cooked_head + 1) % COOKED_QUEUE_SIZE;
    return (unsigned char)c;
}

int keyboard_read_char(void)
{
    int c;
    while ((c = keyboard_try_read_char()) < 0)
        __asm__ volatile ("hlt");
    return c;
}

int keyboard_try_read_raw(unsigned char* scancode, int* extended, char* ascii)
{
    if (raw_head == raw_tail)
        return 0;

    *scancode = raw_queue[raw_head].scancode;
    *extended = raw_queue[raw_head].extended;
    *ascii    = raw_queue[raw_head].ascii;
    raw_head = (raw_head + 1) % RAW_QUEUE_SIZE;
    return 1;
}

void keyboard_flush_raw(void)
{
    raw_head = raw_tail;
}

void keyboard_flush_cooked(void)
{
    cooked_head = cooked_tail;
}
