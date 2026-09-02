/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-20
Date Last Modified: 2026-03-10
Module: PAL (Platform Abstraction Layer)
File: pal.h
About: Platform-independent console abstraction layer.
       ONLY module allowed to use standard C library.
Revisions:
- 2026-02-20  Initial console abstraction
- 2026-02-21  Added string wrapper API
- 2026-02-21  Added file abstraction API
- 2026-02-22  Added pal_readline and pal_get_bg
- 2026-03-10  Added raw key input, terminal size query,
              cursor hide/show, memory allocation wrappers,
              and extra string helpers required by notepad.
- 2026-08-26  Added pal_time_seed() for seeding pal_srand() with
              something that varies run-to-run (needed for
              password-hash salting in utilities/password_hash.c).
------------------------------------------------------------
*/

#ifndef PAL_H
#define PAL_H

#include <stddef.h>

/* ================= PLATFORM ================= */

#ifdef _WIN32
#define OE_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#else
#define OE_PLATFORM_LINUX
#endif

/* ================= INIT ================= */

void pal_init(void);

/* ================= CONSOLE ================= */

void pal_print(const char* text);
void pal_println(const char* text);
void pal_putchar(char c);
char pal_getchar(void);
void pal_pause(void);
void pal_clear_screen(void);
void pal_set_cursor(int row, int col);
void pal_sleep(double seconds);
void pal_readline(char* buffer, int max_len);
void pal_exit(void);

/* Restarts the environment. On the kernel backend this is a real warm
   reboot (hal_reboot()) -- the machine actually restarts. On the hosted
   Windows/Linux backends there is no real "OS" to restart underneath this
   app, so this just ends the process (exit(0)), same as pal_exit() --
   deliberately NOT touching the real host machine's power state. */
void pal_reboot(void);

/* 1 on the kernel backend (this process IS the operating system), 0 on the
   hosted Windows/Linux backends (this process is an app running under a
   real OS). Lets shared code (e.g. setup/src/installer.c) make a real-
   reboot-vs-just-continue decision without a compile-time #ifdef guard in
   code that's built for every backend. */
int pal_is_bare_metal(void);

/* ================= RAW INPUT ================= */

/*
Enter raw / unbuffered key-input mode.
Must be paired with pal_raw_exit() before returning to normal OE use.
*/
void pal_raw_enter(void);

/* Leave raw input mode and restore normal terminal state. */
void pal_raw_exit(void);

/*
Read one keypress in raw mode.  Returns:
  plain ASCII byte          – printable / control character
  '\b'                      – Backspace (normalised from 0x7f on Linux)
  0xFF00 | PAL_SC_xxx       – extended keys (arrows, Del, PgUp, F-keys)
  PAL_KEY_RESIZE (0xFE00)   – synthetic terminal-resize event (Linux)
  0                         – no key within ~100 ms timeout
*/
int  pal_raw_getkey(void);

/* Hide / show the hardware cursor */
void pal_hide_cursor(void);
void pal_show_cursor(void);

/* Query current terminal dimensions */
void pal_get_term_size(int* rows, int* cols);

/* ================= STRING WRAPPERS ================= */

size_t pal_strlen(const char* str);
int    pal_strcmp(const char* s1, const char* s2);
int    pal_strncmp(const char* s1, const char* s2, int n);
char* pal_strcpy(char* dest, const char* src);
char* pal_strncpy(char* dest, const char* src, int n);
int    pal_atoi(const char* str);
void   pal_itoa(int value, char* buffer);
char* pal_strcat(char* dest, const char* src);
int    pal_strnicmp(const char* s1, const char* s2, int n);  /* case-insensitive n-char compare */
char* pal_strchr(const char* s, int c);                     /* find char in string */
char* pal_strdup(const char* s);                            /* heap-duplicate a string */

/* ================= MEMORY ================= */

void* pal_alloc(int size);
void* pal_realloc(void* ptr, int size);
void   pal_free(void* ptr);
void   pal_memmove(void* dst, const void* src, int n);
void   pal_memset(void* dst, int val, int n);
void   pal_memcpy(void* dst, const void* src, int n);

/* ================= RANDOM NUMBER GENERATOR ================= */

void pal_srand(unsigned int seed);
int  pal_rand(void);

/* Wall-clock based value that varies run-to-run — use to seed
   pal_srand() when unpredictability matters (e.g. salting a
   password hash). Not a source of cryptographic randomness by
   itself; pal_rand() is a plain LCG. */
unsigned int pal_time_seed(void);

/* ================= FILE API ================= */

typedef struct pal_file pal_file_t;

pal_file_t* pal_file_open_read(const char* filename);
pal_file_t* pal_file_open_write(const char* filename);

int  pal_file_read(pal_file_t* file, void* buffer, int size);
int  pal_file_write(pal_file_t* file, const void* buffer, int size);

void pal_file_close(pal_file_t* file);
int  pal_file_exists(const char* filename);

/* ================= BACKGROUND COLORS ================= */

const char* pal_get_bg(int color);

/* ================= STYLE MACROS ================= */

/* Text styles */
#define bold        "\033[1m"
#define underline   "\033[4m"
#define reset       "\033[0m"
#define invisible   "\x1B[8m"

/* Foreground colors */
#define red     "\033[31m"
#define green   "\033[32m"
#define yellow  "\033[33m"
#define blue    "\033[34m"
#define purple  "\033[35m"
#define cyan    "\033[36m"
#define white   "\033[37m"

/* Bright foreground */
#define RED     "\033[91m"
#define GREEN   "\033[92m"
#define YELLOW  "\033[93m"
#define BLUE    "\033[94m"

/* Background colors (B1-B16) */
#define B1  "\x1B[41m"  //red
#define B2  "\x1B[42m"  //green
#define B3  "\x1B[43m"  //yellow
#define B4  "\x1B[44m"  //blue
#define B5  "\x1B[45m"  //purple
#define B6  "\x1B[46m"  //cyan
#define B7  "\x1B[47m"  //light grey
#define B8  "\x1B[100m" //grey
#define B9  "\x1B[101m" //light red
#define B10 "\x1B[102m" //light green
#define B11 "\x1B[103m" //light yellow
#define B12 "\x1B[104m" //light blue
#define B13 "\x1B[105m" //light purple
#define B14 "\x1B[106m" //light cyan
#define B15 "\x1B[107m" //white
#define B16 "\x1B[99m"  // blank (no color)

/* Plain black background */
#define BG_BLACK "\x1B[40m"

/* ================= ECHO CONTROL ================= */
//void pal_set_echo(int enable);
//char pal_getchar_raw(void);

/* ================= SCAN CODE CONSTANTS ================= */
/*
Extended keys are encoded as 0xFF00 | scan_code.
Use PAL_KEY_IS_EXT(k) to test, PAL_KEY_SC(k) to extract.
*/
#define PAL_SC_UP     72
#define PAL_SC_DOWN   80
#define PAL_SC_LEFT   75
#define PAL_SC_RIGHT  77
#define PAL_SC_HOME   71
#define PAL_SC_END    79
#define PAL_SC_PGUP   73
#define PAL_SC_PGDN   81
#define PAL_SC_INS    82
#define PAL_SC_DEL    83
#define PAL_SC_F10    68

#define PAL_KEY_IS_EXT(k)       (((k) & 0xFF00) == 0xFF00)
#define PAL_KEY_SC(k)           ((k) & 0x00FF)
#define PAL_KEY_IS_BACKSPACE(k) ((k)=='\b' || (k)==127)
#define PAL_KEY_RESIZE          0xFE00

/* ================= BOOLEAN TYPE ================= */

#ifndef OE_BOOL_DEFINED
#define OE_BOOL_DEFINED
typedef enum { false = 0, true = 1 } bool;
#endif

#endif