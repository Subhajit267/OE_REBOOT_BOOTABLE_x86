/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-20
Date Last Modified: 2026-03-10
Module: PAL
File: pal_linux.c
About: Linux implementation of PAL with echo control.
Revisions:
- 2026-02-20  Initial ANSI console implementation
- 2026-02-21  Added string wrapper functions
- 2026-02-21  Added file abstraction API
- 2026-02-22  Added pal_readline, pal_get_bg
- 2026-02-23  Added pal_set_echo, pal_getchar_raw, and pal_srand/pal_rand
- 2026-03-03  Added support for math functions like pow, floor, fmod, atof, ftoa
- 2026-03-03  Added pal_get_oe_info implementation for systeminfo tool
- 2026-03-10  BUG FIX: Moved _POSIX_C_SOURCE before all #includes
- 2026-03-10  BUG FIX: Strip trailing newline from cpu_name after fgets()
- 2026-03-10  Added pal_raw_enter/exit/getkey, pal_hide/show_cursor,
              pal_get_term_size, memory wrappers, extra string helpers
- 2026-08-25  BUG FIX: pal_get_oe_info() left info->cpu_name
              uninitialized when /proc/cpuinfo couldn't be opened or
              had no "model name" line, and copied the model-name
              value into the fixed char cpu_name[128] with an
              unbounded pal_strcpy(). Defaulted cpu_name up front and
              switched to a bounded pal_strncpy().
- 2026-08-26  Added pal_time_seed() (needed for password-hash
              salting).
------------------------------------------------------------
*/

/* Must be defined before any #include to expose POSIX.1-2008 APIs */
#define _POSIX_C_SOURCE 200809L

#include "pal.h"
#include "pal_math.h"
#include "pal_oe_info.h"
#ifdef OE_PLATFORM_LINUX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <math.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <time.h>
#include <ctype.h>
struct pal_file
{
    FILE* fp;
};
static unsigned long long oe_start_time = 0;
static struct termios original_termios;
static int termios_saved = 0;
/* Random generator state */
static unsigned long next = 1;

/* ================= INIT ================= */
void pal_init(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    oe_start_time = ts.tv_sec;
}

/* ================= CONSOLE ================= */
void pal_print(const char* text)
{
    fputs(text, stdout);
    fflush(stdout);
}
void pal_println(const char* text)
{
    fputs(text, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}
void pal_putchar(char c)
{
    fputc(c, stdout);
    fflush(stdout);
}

char pal_getchar(void) { return (char)getchar(); }
// char pal_getchar_raw(void) { return getchar(); }

void pal_pause(void)
{
    while (getchar() != '\n')
        ;
}
void pal_clear_screen(void)
{
    write(STDOUT_FILENO, "\033[2J\033[H", 7);
}
void pal_set_cursor(int row, int col)
{
    char buf[24];
    int n = snprintf(buf, sizeof(buf), "\033[%d;%dH", row, col);
    write(STDOUT_FILENO, buf, n);
}
void pal_sleep(double seconds)
{
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000L);
    nanosleep(&ts, NULL);
}

void pal_readline(char* buffer, int max_len)
{
    int i = 0;
    char c;
    while (i < max_len - 1)
    {
        c = pal_getchar();
        if (c == '\n' || c == '\r')
            break;
        if (c == 8 || c == 127)
        {
            if (i > 0)
            {
                i--;
                write(STDOUT_FILENO, "\b \b", 3);
            }
        }
        else
        {
            buffer[i++] = c;
            // putchar(c); fflush(stdout);
        }
    }
    buffer[i] = '\0';
    fputc('\n', stdout);
    fflush(stdout);
}

/* ================= CURSOR ================= */
void pal_hide_cursor(void) { write(STDOUT_FILENO, "\033[?25l", 6); }
void pal_show_cursor(void) { write(STDOUT_FILENO, "\033[?25h", 6); }

/* ================= TERMINAL SIZE ================= */
void pal_get_term_size(int* rows, int* cols)
{
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    *rows = ws.ws_row;
    *cols = ws.ws_col;
}

/* ================= RAW INPUT ================= */

static volatile int pal_resize_pending = 0;

static void pal_sigwinch(int sig) { (void)sig; pal_resize_pending = 1; }

void pal_raw_enter(void)
{
    tcgetattr(STDIN_FILENO, &original_termios);
    termios_saved = 1;

    struct termios raw = original_termios;
    raw.c_iflag &= ~(unsigned)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(unsigned)(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;   /* 100 ms timeout for non-blocking poll */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    signal(SIGWINCH, pal_sigwinch);
    write(STDOUT_FILENO, "\033[?7l", 5);   /* disable auto-wrap */
    pal_hide_cursor();
}

void pal_raw_exit(void)
{
    if (termios_saved)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
    signal(SIGWINCH, SIG_DFL);
    write(STDOUT_FILENO, "\033[?7h", 5);   /* re-enable auto-wrap */
    write(STDOUT_FILENO, "\033[0m", 4);
    pal_show_cursor();
    termios_saved = 0;
}

int pal_raw_getkey(void)
{
    if (pal_resize_pending)
    {
        pal_resize_pending = 0;
        return PAL_KEY_RESIZE;
    }

    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
    {
        if (pal_resize_pending) { pal_resize_pending = 0; return PAL_KEY_RESIZE; }
        return 0;
    }

    if (c == 0x7f) return '\b';       /* Backspace */
    if (c != '\033') return (int)c;   /* normal key */

    /* -------- ESC sequence -------- */
    unsigned char seq[8];
    int n = 0;
    while (n < 6)
    {
        if (read(STDIN_FILENO, &seq[n], 1) != 1) break;
        n++;
        if (seq[n - 1] == '~' || isalpha(seq[n - 1])) break;
    }
    if (n == 0) return '\033';

    /* CSI: ESC [ ... */
    if (seq[0] == '[')
    {
        if (n < 2) return 0;
        switch (seq[1])
        {
        case 'A': return 0xFF00 | PAL_SC_UP;
        case 'B': return 0xFF00 | PAL_SC_DOWN;
        case 'C': return 0xFF00 | PAL_SC_RIGHT;
        case 'D': return 0xFF00 | PAL_SC_LEFT;
        case 'H': return 0xFF00 | PAL_SC_HOME;
        case 'F': return 0xFF00 | PAL_SC_END;
        case '1':
            if (n >= 3 && seq[2] == '~') return 0xFF00 | PAL_SC_HOME;
            if (n >= 4 && seq[3] == '~' &&
                (seq[2] == '7' || seq[2] == '8' || seq[2] == '9'))
                return 0xFF00 | PAL_SC_F10;
            if (n >= 4 && seq[2] == '9' && seq[3] == '~') return 0xFF00 | PAL_SC_F10;
            return 0;
        case '2':
            if (n >= 3 && seq[2] == '~')              return 0xFF00 | PAL_SC_INS;
            if (n >= 4 && seq[2] == '1' && seq[3] == '~') return 0xFF00 | PAL_SC_F10;
            return 0;
        case '3': if (n >= 3 && seq[2] == '~') return 0xFF00 | PAL_SC_DEL;  return 0;
        case '4': if (n >= 3 && seq[2] == '~') return 0xFF00 | PAL_SC_END;  return 0;
        case '5': if (n >= 3 && seq[2] == '~') return 0xFF00 | PAL_SC_PGUP; return 0;
        case '6': if (n >= 3 && seq[2] == '~') return 0xFF00 | PAL_SC_PGDN; return 0;
        default:  return 0;
        }
    }

    /* SS3: ESC O ... */
    if (seq[0] == 'O')
    {
        if (n < 2) return 0;
        switch (seq[1])
        {
        case 'P': return 0xFF00 | PAL_SC_F10;
        case 'H': return 0xFF00 | PAL_SC_HOME;
        case 'F': return 0xFF00 | PAL_SC_END;
        default:  return 0;
        }
    }

    return 0;
}

/* ================= ECHO CONTROL ================= */
// void pal_set_echo(int enable)
//{
//     if (!termios_saved) {
//         tcgetattr(STDIN_FILENO, &original_termios);
//         termios_saved = 1;
//     }
//
//     struct termios newt = original_termios;
//     if (enable)
//         newt.c_lflag |= ECHO;
//     else
//         newt.c_lflag &= ~ECHO;
//
//     tcsetattr(STDIN_FILENO, TCSANOW, &newt);
// }

/* ================= STRING ================= */
size_t pal_strlen(const char* str) { return strlen(str); }
int    pal_strcmp(const char* s1, const char* s2) { return strcmp(s1, s2); }
int    pal_strncmp(const char* s1, const char* s2, int n) { return strncmp(s1, s2, (size_t)n); }
char* pal_strcpy(char* dest, const char* src) { return strcpy(dest, src); }
char* pal_strncpy(char* dest, const char* src, int n)
{
    int i;
    for (i = 0; i < n - 1 && src[i]; i++) dest[i] = src[i];
    dest[i] = '\0';
    return dest;
}
int    pal_atoi(const char* str) { return atoi(str); }
void   pal_itoa(int value, char* buffer) { sprintf(buffer, "%d", value); }
char* pal_strcat(char* dest, const char* src) { return strcat(dest, src); }

int pal_strnicmp(const char* s1, const char* s2, int n)
{
    return strncasecmp(s1, s2, (size_t)n);
}

char* pal_strchr(const char* s, int c)
{
    return strchr(s, c);
}

char* pal_strdup(const char* s)
{
    int   len = (int)strlen(s);
    char* copy = malloc(len + 1);
    if (copy) strcpy(copy, s);
    return copy;
}

/* ================= MEMORY ================= */
void* pal_alloc(int size) { return malloc((size_t)size); }
void* pal_realloc(void* ptr, int size) { return realloc(ptr, (size_t)size); }
void  pal_free(void* ptr) { free(ptr); }
void  pal_memmove(void* dst, const void* src, int n) { memmove(dst, src, (size_t)n); }
void  pal_memset(void* dst, int val, int n) { memset(dst, val, (size_t)n); }
void  pal_memcpy(void* dst, const void* src, int n) { memcpy(dst, src, (size_t)n); }

/* ================= RANDOM ================= */
void pal_srand(unsigned int seed) { next = seed; }
int pal_rand(void)
{
    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}
unsigned int pal_time_seed(void) { return (unsigned int)time(NULL); }

/* ================= FILE ================= */
pal_file_t* pal_file_open_read(const char* filename)
{
    pal_file_t* f = malloc(sizeof(pal_file_t));
    if (!f) return NULL;
    f->fp = fopen(filename, "rb");
    if (!f->fp) { free(f); return NULL; }
    return f;
}
pal_file_t* pal_file_open_write(const char* filename)
{
    pal_file_t* f = malloc(sizeof(pal_file_t));
    if (!f) return NULL;
    f->fp = fopen(filename, "wb");
    if (!f->fp) { free(f); return NULL; }
    return f;
}
int pal_file_read(pal_file_t* file, void* buffer, int size)
{
    return (int)fread(buffer, 1, (size_t)size, file->fp);
}
int pal_file_write(pal_file_t* file, const void* buffer, int size)
{
    return (int)fwrite(buffer, 1, (size_t)size, file->fp);
}
void pal_file_close(pal_file_t* file)
{
    if (file) { fclose(file->fp); free(file); }
}
int pal_file_exists(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* ================= BACKGROUND COLORS ================= */
const char* pal_get_bg(int color)
{
    static const char* codes[] = {
        "", "\x1B[41m", "\x1B[42m", "\x1B[43m", "\x1B[44m",
        "\x1B[45m", "\x1B[46m", "\x1B[47m", "\x1B[100m",
        "\x1B[101m", "\x1B[102m", "\x1B[103m", "\x1B[104m",
        "\x1B[105m", "\x1B[106m", "\x1B[107m", "\x1B[99m" };
    if (color < 1 || color > 16) return "\x1B[44m";
    return codes[color];
}

// EXIt
void pal_exit(void) { exit(0); }
void pal_reboot(void) { exit(0); } /* no real "OS" to restart in a hosted build -- deliberately does not touch the real machine's power state */
int pal_is_bare_metal(void) { return 0; }

/* ================= MATH ================= */
double pal_pow(double base, double exp) { return pow(base, exp); }
double pal_floor(double x) { return floor(x); }
double pal_fmod(double a, double b) { return fmod(a, b); }
double pal_atof(const char* str) { return strtod(str, NULL); }
void pal_ftoa(double value, char* buffer, int precision)
{
    sprintf(buffer, "%.*f", precision, value);
}

/* ================= OE INFO ================= */
int pal_get_oe_info(pal_oe_info_t* info)
{
    if (!info) return 0;

    /* -------- CPU CORES -------- */
    info->cpu_cores = ((int)sysconf(_SC_NPROCESSORS_ONLN)) / 2;
    info->cpu_threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    pal_strcpy(info->architecture, "x64");

    /* -------- CPU NAME + MHz -------- */
    pal_strcpy(info->cpu_name, "Unknown CPU");
    FILE* f = fopen("/proc/cpuinfo", "r");
    if (f)
    {
        char line[256];
        while (fgets(line, sizeof(line), f))
        {
            if (strncmp(line, "model name", 10) == 0)
            {
                char* colon = strchr(line, ':');
                if (colon)
                {
                    pal_strncpy(info->cpu_name, colon + 2, sizeof(info->cpu_name));
                    /* Strip trailing newline left by fgets */
                    int len = (int)strlen(info->cpu_name);
                    while (len > 0 &&
                        (info->cpu_name[len - 1] == '\n' ||
                            info->cpu_name[len - 1] == '\r' ||
                            info->cpu_name[len - 1] == ' '))
                        info->cpu_name[--len] = '\0';
                }
                break;
            }
        }
        fclose(f);
    }
    FILE* f1 = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (f1)
    {
        long khz = 0;
        fscanf(f1, "%ld", &khz);
        fclose(f1);
        info->cpu_mhz = khz / 1000; /* convert kHz -> MHz */
    }
    else
    {
        info->cpu_mhz = 0;
    }

    /* -------- MEMORY -------- */
    struct sysinfo memInfo;
    if (sysinfo(&memInfo) != 0) return 0;
    info->total_ram = (unsigned long long)memInfo.totalram * memInfo.mem_unit;
    info->free_ram = (unsigned long long)memInfo.freeram * memInfo.mem_unit;
    info->backend_name = "Linux PAL";

    /* -------- DISK INFO -------- */
    struct statvfs fs;
    if (statvfs("/", &fs) == 0)
    {
        info->disk_total = (unsigned long long)fs.f_blocks * fs.f_frsize;
        info->disk_free = (unsigned long long)fs.f_bfree * fs.f_frsize;
    }

    /* -------- UPTIME -------- */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    info->uptime_seconds = now.tv_sec - oe_start_time;

    return 1;
}

#endif