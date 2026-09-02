/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-20
Date Last Modified: 2026-03-10
Module: PAL
File: pal_windows.c
About: Windows implementation of PAL with echo control.
Revisions:
- 2026-02-20  Initial VT console implementation
- 2026-02-21  Added string wrapper functions
- 2026-02-21  Added file abstraction API
- 2026-02-22  Added pal_readline, pal_get_bg
- 2026-02-23  Added pal_set_echo, pal_getchar_raw, and pal_srand/pal_rand
- 2026-03-03  Added support for math functions like pow, floor, fmod, atof, ftoa
- 2026-03-03  Added pal_get_oe_info implementation for systeminfo tool
- 2026-03-10  Added pal_raw_enter/exit/getkey, pal_hide/show_cursor,
              pal_get_term_size, memory wrappers, extra string helpers
- 2026-08-25  BUG FIX: pal_get_oe_info() never checked
              RegQueryValueExA()'s return value — if the CPU name/MHz
              registry values were missing, info->cpu_name/cpu_mhz
              were left holding uninitialized stack garbage. Now
              defaulted before the read and only overwritten on
              success.
- 2026-08-26  Added pal_time_seed() (needed for password-hash
              salting).
ALL CHECKED AND WORKING
------------------------------------------------------------
*/

#define _CRT_SECURE_NO_WARNINGS
#include "pal.h"
#include "pal_math.h"
#include "pal_oe_info.h"
#ifdef OE_PLATFORM_WINDOWS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <winreg.h>
#include <conio.h>
struct pal_file { FILE* fp; };
static ULONGLONG oe_start_time = 0;
/* Random generator state */
static unsigned long next = 1;

/* ================= INIT ================= */
void pal_init(void)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    oe_start_time = GetTickCount64();
    if (GetConsoleMode(hOut, &mode))
    {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, mode);
    }
    //pal_set_echo(0);
}

/* ================= CONSOLE ================= */
void pal_print(const char* text) { printf("%s", text);  fflush(stdout); }
void pal_println(const char* text) { printf("%s\n", text); fflush(stdout); }
void pal_putchar(char c) { putchar(c); fflush(stdout); }

char pal_getchar(void) { return (char)getchar(); }
//char pal_getchar_raw(void) { return getchar(); }

void pal_pause(void) { while (getchar() != '\n'); }
void pal_clear_screen(void) { system("cls"); }
void pal_set_cursor(int row, int col) { printf("\033[%d;%dH", row, col); fflush(stdout); }
void pal_sleep(double seconds) { Sleep((DWORD)(seconds * 1000)); }

void pal_readline(char* buffer, int max_len)
{
    int i = 0;
    char c;
    while (i < max_len - 1) {
        c = pal_getchar();
        if (c == '\n' || c == '\r') break;
        if (c == 8 || c == 127) {
            if (i > 0) { i--; printf("\b \b"); fflush(stdout); }
        }
        else {
            buffer[i++] = c;
            //putchar(c); fflush(stdout);
        }
    }
    buffer[i] = '\0';
    putchar('\n');
    fflush(stdout);
}

/* ================= CURSOR ================= */
void pal_hide_cursor(void)
{
    HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci = { 1, FALSE };
    SetConsoleCursorInfo(hCon, &ci);
}

void pal_show_cursor(void)
{
    HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci = { 1, TRUE };
    SetConsoleCursorInfo(hCon, &ci);
}

/* ================= TERMINAL SIZE ================= */
void pal_get_term_size(int* rows, int* cols)
{
    HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hCon, &csbi);
    *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
}

/* ================= RAW INPUT ================= */
void pal_raw_enter(void)
{
    /* Windows VT mode is already enabled by pal_init().
       _getch() is inherently raw – nothing extra needed. */
    pal_hide_cursor();
}

void pal_raw_exit(void)
{
    pal_show_cursor();
    printf("\033[0m");
    fflush(stdout);
}

int pal_raw_getkey(void)
{
    /* _kbhit returns 0 if no key is available (non-blocking ~100ms equiv) */
    if (!_kbhit()) return 0;
    int ch = _getch();
    if (ch == 0 || ch == 0xE0) return 0xFF00 | _getch();   /* extended key */
    if (ch == 8)               return '\b';                  /* normalise Backspace */
    return ch;
}

///* ================= ECHO CONTROL ================= */
//void pal_set_echo(int enable)
//{
//    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
//    DWORD mode;
//    GetConsoleMode(hStdin, &mode);
//    if (enable)
//        mode |= ENABLE_ECHO_INPUT;
//    else
//        mode &= ~ENABLE_ECHO_INPUT;
//    SetConsoleMode(hStdin, mode);
//}

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
    return _strnicmp(s1, s2, (size_t)n);
}

char* pal_strchr(const char* s, int c)
{
    return strchr(s, c);
}

char* pal_strdup(const char* s)
{
    int   len = (int)strlen(s);
    char* copy = (char*)malloc(len + 1);
    if (copy) strcpy(copy, s);
    return copy;
}

/* ================= MEMORY ================= */
void* pal_alloc(int size) { return malloc((size_t)size); }
void* pal_realloc(void* ptr, int size) { return realloc(ptr, (size_t)size); }
void   pal_free(void* ptr) { free(ptr); }
void   pal_memmove(void* dst, const void* src, int n) { memmove(dst, src, (size_t)n); }
void   pal_memset(void* dst, int val, int n) { memset(dst, val, (size_t)n); }
void   pal_memcpy(void* dst, const void* src, int n) { memcpy(dst, src, (size_t)n); }

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
    pal_file_t* f = (pal_file_t*)malloc(sizeof(pal_file_t));
    if (!f) return NULL;
    f->fp = fopen(filename, "rb");
    if (!f->fp) { free(f); return NULL; }
    return f;
}
pal_file_t* pal_file_open_write(const char* filename)
{
    pal_file_t* f = (pal_file_t*)malloc(sizeof(pal_file_t));
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
void pal_file_close(pal_file_t* file) { if (file) { fclose(file->fp); free(file); } }
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
        "\x1B[105m", "\x1B[106m", "\x1B[107m", "\x1B[99m"
    };
    if (color < 1 || color > 16) return "\x1B[44m";
    return codes[color];
}

//EXIt
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

    info->disk_total = 0;
    info->disk_free = 0;
    /* -------- CPU THREADS AND CORES -------- */
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    info->cpu_threads = (int)sysInfo.dwNumberOfProcessors;
    info->cpu_cores = ((int)sysInfo.dwNumberOfProcessors) / 2;

#ifdef _WIN64
    pal_strcpy(info->architecture, "x64");
#else
    pal_strcpy(info->architecture, "x86");
#endif

    /* -------- CPU NAME + MHz -------- */
    pal_strcpy(info->cpu_name, "Unknown CPU");
    info->cpu_mhz = 0;

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD size = sizeof(info->cpu_name);
        if (RegQueryValueExA(hKey, "ProcessorNameString", NULL,
            NULL, (LPBYTE)info->cpu_name, &size) != ERROR_SUCCESS)
        {
            pal_strcpy(info->cpu_name, "Unknown CPU");
        }

        DWORD mhz = 0;
        size = sizeof(DWORD);
        if (RegQueryValueExA(hKey, "~MHz", NULL,
            NULL, (LPBYTE)&mhz, &size) == ERROR_SUCCESS)
        {
            info->cpu_mhz = mhz;
        }
        RegCloseKey(hKey);
    }

    /* -------- MEMORY -------- */
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);
    if (!GlobalMemoryStatusEx(&memInfo)) return 0;
    info->total_ram = memInfo.ullTotalPhys;
    info->free_ram = memInfo.ullAvailPhys;
    info->backend_name = "Windows PAL";

    /* -------- DISK INFO -------- */
    DWORD driveMask = GetLogicalDrives();
    char letter;
    for (letter = 'A'; letter <= 'Z'; letter++)
    {
        if (driveMask & (1 << (letter - 'A')))
        {
            char rootPath[4] = { letter, ':', '\\', '\0' };
            ULARGE_INTEGER totalBytes, freeBytes;
            if (GetDiskFreeSpaceExA(rootPath, NULL, &totalBytes, &freeBytes))
            {
                info->disk_total += totalBytes.QuadPart;
                info->disk_free += freeBytes.QuadPart;
            }
        }
    }

    /* -------- UPTIME -------- */
    ULONGLONG now = GetTickCount64();
    info->uptime_seconds = (now - oe_start_time) / 1000ULL;

    return 1;
}

#endif