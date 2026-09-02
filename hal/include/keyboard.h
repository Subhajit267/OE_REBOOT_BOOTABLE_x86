/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-09-01
Module: HAL
File: keyboard.h
About: PS/2 keyboard driver (IRQ1, scancode set 1, US QWERTY). Feeds two
       independent queues from every keystroke: a cooked ASCII queue for
       pal_kernel.c's pal_getchar()/pal_readline(), and a raw queue
       (undecoded scancode + 0xE0 flag + best-effort ASCII) for
       pal_raw_getkey() (notepad's editor).
Revisions:
- 2026-08-30  Initial creation (Phase 2): cooked queue only, direct
              IRQ1 echo.
- 2026-08-30  Raw queue added (Phase 6), real input queueing replacing
              direct echo (echo moved to pal_kernel.c to match real
              cooked/raw terminal semantics).
- 2026-09-01  Added keyboard_flush_raw()/keyboard_flush_cooked() -- both
              queues are fed unconditionally regardless of which mode is
              "active", so switching modes (pal_raw_enter()/
              pal_raw_exit()) without flushing let stale keystrokes from
              one mode replay into the other (found via notepad opening
              a file and showing the just-typed launch command as
              garbage leading text in the document).
------------------------------------------------------------
*/

#ifndef HAL_KEYBOARD_H
#define HAL_KEYBOARD_H

/* Registers the IRQ1 handler for the PS/2 keyboard controller. Call after
   irq_install()/pic_remap(), before enabling interrupts (sti). */
void keyboard_initialize(void);

/* Cooked queue: mapped ASCII characters only (shift already applied),
   key-release events and unmapped keys are dropped here -- this is meant
   for pal_kernel.c's cooked-mode pal_getchar()/pal_readline(). */
int keyboard_read_char(void);     /* blocks (hlt-spins) until a char is available */
int keyboard_try_read_char(void); /* returns -1 immediately if none is waiting */

/* Raw queue: every make-code byte, undecoded, plus whether it arrived
   after an 0xE0 prefix, plus the same shift-aware ASCII translation the
   cooked queue uses (0 if the key has no ASCII mapping -- extended keys
   never do). Populated even while the cooked queue is being fed, so both
   modes see input; it's pal_raw_enter()/pal_raw_exit() that decides which
   queue a caller should be draining. pal_kernel.c's pal_raw_getkey() uses
   `ascii` directly for plain keys and maps (scancode, extended) onto its
   own PAL_SC_xxx constants for the rest -- that numbering is a PAL
   contract detail, not something hal/ should know about. */
int keyboard_try_read_raw(unsigned char* scancode, int* extended, char* ascii);

/* keyboard_handler() feeds both queues from every keystroke unconditionally
   (see keyboard_try_read_raw()'s comment) -- whichever queue a caller isn't
   draining silently accumulates a backlog of "current mode" keystrokes that
   the *other* mode would otherwise replay the instant it starts reading.
   Call these right at a cooked/raw mode transition (pal_raw_enter()/
   pal_raw_exit() in pal_kernel.c) to discard that backlog instead of
   replaying it. */
void keyboard_flush_raw(void);
void keyboard_flush_cooked(void);

#endif
