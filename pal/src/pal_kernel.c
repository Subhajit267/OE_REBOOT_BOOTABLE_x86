/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: PAL
File: pal_kernel.c
About: Bare-metal PAL backend -- satisfies pal.h (plus pal_math.h,
       pal_oe_info.h, pal_dir_file_cmds.h) directly against the kernel's
       own hal.h/fat.h/heap.h instead of an OS. Ring-0-only,
       single-tasking (per the agreed v1 kernel scope), so these are
       plain function calls, not trap-based syscalls; a ring3 boundary
       would turn this same contract into real syscalls later without
       userland needing to change.

       pal_pow()'s general case (non-whole-number exponent) uses a
       hand-rolled ln/exp series (range-reduced Taylor expansion) since
       there's no libm -- accurate to several decimal places, not
       IEEE-754-exact. Whole-number exponents use exact repeated
       squaring instead and don't go through that path.

       pal_get_oe_info()'s cpu_mhz is always 0 -- no calibrated
       TSC/PIT-based frequency measurement yet, and reporting a guess
       would be worse than an honest "unknown".
Revisions:
- 2026-08-30  Initial creation (Phase 6): console/string/memory/file
              primitives, pal_math.h, pal_oe_info.h. Directory ops
              (mdr/rdr/cpdr/rnmdr/mvdr) failed closed -- fat.c was
              root-directory-only, no subdirectory tree to operate on.
- 2026-08-30  Full port completed (Phase 7): every pal_dir_file_cmds.h
              function now real (scoped to fat.c's then-flat root
              directory) once fat.c gained write support the same day.
- 2026-08-31  Real subdirectory support: current_dir/current_path state
              added, mdr/rdr/cdr/pwd/ldr/cpf/mvf/rnmf/rmf/rdf became
              path-aware via fat.c's new fat_resolve_path()/`_in()` API;
              cpdr/mvdr/rnmdr implemented (top-level only, no recursive
              subdirectory copy) and wired into the interactive shell.
------------------------------------------------------------
*/

#include "pal.h"
#include "pal_math.h"
#include "pal_oe_info.h"
#include "pal_dir_file_cmds.h"
#include "hal.h"
#include "fat.h"
#include "heap.h"
#include "pmm.h"

#define RAW_KEY_TIMEOUT_TICKS 10 /* ~100ms at HAL_TIMER_HZ=100, per pal.h's pal_raw_getkey doc */

struct pal_file
{
    unsigned char* data;
    int size;      /* read mode: total bytes available. write mode: bytes accumulated so far. */
    int pos;       /* read cursor */
    int capacity;  /* write mode only: allocated size of data, grows via krealloc */
    int write_mode;
    char filename[64]; /* write mode only: remembered so pal_file_close() can flush it */
};

static unsigned long rand_state = 1;

/* ================= INIT ================= */

static struct fat_dir current_dir;
static char current_path[256] = "/";

void pal_init(void)
{
    /* hal_initialize() already ran before anything could call into this
       backend -- nothing else to bring up. fat_mount() (called from
       kernel_main's boot_diagnostics(), before pal_init()) has already
       succeeded by this point, so the root directory is safe to read here. */
    fat_get_root_dir(&current_dir);
    current_path[0] = '/';
    current_path[1] = '\0';
}

/* ================= CONSOLE ================= */

void pal_print(const char* text)
{
    hal_console_print(text);
}

void pal_println(const char* text)
{
    hal_console_print(text);
    hal_console_putchar('\n');
}

void pal_putchar(char c)
{
    hal_console_putchar(c);
}

char pal_getchar(void)
{
    return (char)hal_keyboard_read_char();
}

void pal_pause(void)
{
    int c;
    do { c = hal_keyboard_read_char(); } while (c != '\n');
}

void pal_clear_screen(void)
{
    hal_console_print("\033[2J\033[H");
}

static int uint_to_str(unsigned int val, char* out)
{
    char tmp[10];
    int n = 0, i;

    if (val == 0)
    {
        out[0] = '0';
        return 1;
    }
    while (val > 0)
    {
        tmp[n++] = (char)('0' + val % 10);
        val /= 10;
    }
    for (i = 0; i < n; i++)
        out[i] = tmp[n - 1 - i];
    return n;
}

void pal_set_cursor(int row, int col)
{
    char buf[24];
    int n = 0;

    buf[n++] = '\033';
    buf[n++] = '[';
    n += uint_to_str((unsigned int)row, buf + n);
    buf[n++] = ';';
    n += uint_to_str((unsigned int)col, buf + n);
    buf[n++] = 'H';
    buf[n] = '\0';

    hal_console_print(buf);
}

void pal_sleep(double seconds)
{
    unsigned long start = hal_timer_get_ticks();
    unsigned long ticks_to_wait = (unsigned long)(seconds * HAL_TIMER_HZ);

    while (hal_timer_get_ticks() - start < ticks_to_wait)
        __asm__ volatile ("hlt");
}

void pal_readline(char* buffer, int max_len)
{
    int i = 0;
    int c;

    while (i < max_len - 1)
    {
        c = hal_keyboard_read_char();
        if (c == '\n' || c == '\r')
            break;
        if (c == '\b' || c == 127)
        {
            if (i > 0)
            {
                i--;
                hal_console_print("\b \b");
            }
            continue;
        }
        buffer[i++] = (char)c;
        hal_console_putchar((char)c);
    }
    buffer[i] = '\0';
    hal_console_putchar('\n');
}

void pal_exit(void)
{
    hal_console_print("\nShutting down.\n");
    __asm__ volatile ("cli");
    hal_power_off(); /* never returns -- tries the real ACPI shutdown ports, falls back to halting the CPU if the host doesn't implement either */
}

void pal_reboot(void)
{
    hal_console_print("\nRestarting.\n");
    __asm__ volatile ("cli");
    hal_reboot(); /* never returns -- real warm reboot via the 8042 controller */
}

int pal_is_bare_metal(void)
{
    return 1;
}

/* ================= RAW INPUT ================= */

void pal_raw_enter(void)
{
    /* keyboard_handler() feeds the cooked queue from every keystroke too
       (see hal/src/keyboard.c), so anything typed at a preceding cooked-mode
       prompt (pal_readline()/pal_getchar()) that happened to also queue up
       raw-side sits here unconsumed until now -- flush it, or the first
       pal_raw_getkey() calls after this would replay that backlog as if
       freshly typed (found via notepad opening with the command that
       launched it showing up as leading garbage text in the document). */
    hal_keyboard_flush_raw();
    hal_console_print("\033[?7l"); /* disable auto-wrap, matches pal_linux.c's raw mode */
    hal_console_print("\033[?25l");
}

void pal_raw_exit(void)
{
    hal_console_print("\033[?7h");
    hal_console_print("\033[0m");
    hal_console_print("\033[?25h");
    /* Symmetric flush: whatever was typed during raw mode also queued up
       cooked-side and would otherwise replay into the very next
       pal_readline()/pal_getchar() after returning here. */
    hal_keyboard_flush_cooked();
}

int pal_raw_getkey(void)
{
    unsigned char scancode;
    int extended;
    char ascii;
    unsigned long start = hal_timer_get_ticks();

    while (!hal_keyboard_try_read_raw(&scancode, &extended, &ascii))
    {
        if (hal_timer_get_ticks() - start >= RAW_KEY_TIMEOUT_TICKS)
            return 0;
        __asm__ volatile ("hlt");
    }

    if (ascii)
        return PAL_KEY_IS_BACKSPACE(ascii) ? '\b' : (int)(unsigned char)ascii;

    if (extended)
    {
        switch (scancode)
        {
        case 0x48: return 0xFF00 | PAL_SC_UP;
        case 0x50: return 0xFF00 | PAL_SC_DOWN;
        case 0x4B: return 0xFF00 | PAL_SC_LEFT;
        case 0x4D: return 0xFF00 | PAL_SC_RIGHT;
        case 0x47: return 0xFF00 | PAL_SC_HOME;
        case 0x4F: return 0xFF00 | PAL_SC_END;
        case 0x49: return 0xFF00 | PAL_SC_PGUP;
        case 0x51: return 0xFF00 | PAL_SC_PGDN;
        case 0x52: return 0xFF00 | PAL_SC_INS;
        case 0x53: return 0xFF00 | PAL_SC_DEL;
        default:   return 0;
        }
    }

    if (scancode == 0x44) /* F10 (non-extended in scancode set 1) */
        return 0xFF00 | PAL_SC_F10;

    return 0; /* unmapped key (ctrl/alt/capslock/numpad/etc.) */
}

void pal_hide_cursor(void) { hal_console_print("\033[?25l"); }
void pal_show_cursor(void) { hal_console_print("\033[?25h"); }

void pal_get_term_size(int* rows, int* cols)
{
    hal_console_get_size(rows, cols);
}

/* ================= STRING WRAPPERS ================= */

size_t pal_strlen(const char* str)
{
    size_t n = 0;
    while (str[n]) n++;
    return n;
}

int pal_strcmp(const char* s1, const char* s2)
{
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int pal_strncmp(const char* s1, const char* s2, int n)
{
    int i;
    for (i = 0; i < n; i++)
    {
        if (s1[i] != s2[i] || s1[i] == '\0')
            return (unsigned char)s1[i] - (unsigned char)s2[i];
    }
    return 0;
}

char* pal_strcpy(char* dest, const char* src)
{
    char* d = dest;
    while ((*d++ = *src++) != '\0') { }
    return dest;
}

char* pal_strncpy(char* dest, const char* src, int n)
{
    int i;
    for (i = 0; i < n - 1 && src[i]; i++) dest[i] = src[i];
    dest[i] = '\0';
    return dest;
}

int pal_atoi(const char* str)
{
    int sign = 1, result = 0;
    while (*str == ' ' || *str == '\t') str++;
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') str++;
    while (*str >= '0' && *str <= '9')
        result = result * 10 + (*str++ - '0');
    return result * sign;
}

void pal_itoa(int value, char* buffer)
{
    long long magnitude = value < 0 ? -(long long)value : (long long)value;
    unsigned int uval = (unsigned int)magnitude; /* safe even for INT_MIN: 2147483648 fits in unsigned int */
    int n = 0;

    if (value < 0)
        buffer[n++] = '-';
    n += uint_to_str(uval, buffer + n);
    buffer[n] = '\0';
}

char* pal_strcat(char* dest, const char* src)
{
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++) != '\0') { }
    return dest;
}

static char to_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

int pal_strnicmp(const char* s1, const char* s2, int n)
{
    int i;
    for (i = 0; i < n; i++)
    {
        char c1 = to_lower(s1[i]);
        char c2 = to_lower(s2[i]);
        if (c1 != c2 || s1[i] == '\0')
            return (unsigned char)c1 - (unsigned char)c2;
    }
    return 0;
}

char* pal_strchr(const char* s, int c)
{
    while (*s)
    {
        if (*s == (char)c)
            return (char*)s;
        s++;
    }
    return (c == '\0') ? (char*)s : 0;
}

char* pal_strdup(const char* s)
{
    size_t len = pal_strlen(s);
    char* copy = (char*)pal_alloc((int)len + 1);
    if (copy)
        pal_strcpy(copy, s);
    return copy;
}

/* ================= MEMORY ================= */

void* pal_alloc(int size)
{
    if (size <= 0)
        return 0;
    return kmalloc((unsigned int)size);
}

void* pal_realloc(void* ptr, int size)
{
    if (size <= 0)
    {
        kfree(ptr);
        return 0;
    }
    return krealloc(ptr, (unsigned int)size);
}

void pal_free(void* ptr)
{
    kfree(ptr);
}

void pal_memmove(void* dst, const void* src, int n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    int i;

    if (d < s)
    {
        for (i = 0; i < n; i++) d[i] = s[i];
    }
    else
    {
        for (i = n - 1; i >= 0; i--) d[i] = s[i];
    }
}

void pal_memset(void* dst, int val, int n)
{
    unsigned char* d = (unsigned char*)dst;
    int i;
    for (i = 0; i < n; i++) d[i] = (unsigned char)val;
}

void pal_memcpy(void* dst, const void* src, int n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    int i;
    for (i = 0; i < n; i++) d[i] = s[i];
}

/* ================= RANDOM NUMBER GENERATOR ================= */

void pal_srand(unsigned int seed)
{
    rand_state = seed;
}

int pal_rand(void)
{
    rand_state = rand_state * 1103515245u + 12345u;
    return (int)((rand_state / 65536u) % 32768u);
}

unsigned int pal_time_seed(void)
{
    return (unsigned int)hal_timer_get_ticks();
}

/* ================= FILE API ================= */

pal_file_t* pal_file_open_read(const char* filename)
{
    pal_file_t* f;
    int size = fat_get_file_size(filename);

    if (size < 0)
        return 0;

    f = (pal_file_t*)kmalloc(sizeof(pal_file_t));
    if (!f)
        return 0;

    f->data = size > 0 ? (unsigned char*)kmalloc((unsigned int)size) : 0;
    if (size > 0 && !f->data)
    {
        kfree(f);
        return 0;
    }

    if (size > 0 && fat_read_file(filename, f->data, (unsigned int)size) != size)
    {
        kfree(f->data);
        kfree(f);
        return 0;
    }

    f->size = size;
    f->pos = 0;
    f->write_mode = 0;
    return f;
}

pal_file_t* pal_file_open_write(const char* filename)
{
    pal_file_t* f = (pal_file_t*)kmalloc(sizeof(pal_file_t));
    if (!f)
        return 0;

    f->capacity = 4096;
    f->data = (unsigned char*)kmalloc((unsigned int)f->capacity);
    if (!f->data)
    {
        kfree(f);
        return 0;
    }

    f->size = 0;
    f->pos = 0;
    f->write_mode = 1;
    pal_strncpy(f->filename, filename, sizeof(f->filename));
    return f;
}

int pal_file_read(pal_file_t* file, void* buffer, int size)
{
    unsigned char* out = (unsigned char*)buffer;
    int available = file->size - file->pos;
    int to_copy = size < available ? size : available;
    int i;

    if (to_copy < 0)
        to_copy = 0;

    for (i = 0; i < to_copy; i++)
        out[i] = file->data[file->pos + i];

    file->pos += to_copy;
    return to_copy;
}

int pal_file_write(pal_file_t* file, const void* buffer, int size)
{
    const unsigned char* src = (const unsigned char*)buffer;
    int i;

    if (!file->write_mode || size <= 0)
        return 0;

    if (file->size + size > file->capacity)
    {
        int new_cap = file->capacity;
        unsigned char* new_data;

        while (new_cap < file->size + size)
            new_cap *= 2;

        new_data = (unsigned char*)krealloc(file->data, (unsigned int)new_cap);
        if (!new_data)
            return 0;
        file->data = new_data;
        file->capacity = new_cap;
    }

    for (i = 0; i < size; i++)
        file->data[file->size + i] = src[i];
    file->size += size;
    return size;
}

void pal_file_close(pal_file_t* file)
{
    if (!file)
        return;
    if (file->write_mode)
        fat_write_file(file->filename, file->data, (unsigned int)file->size);
    kfree(file->data);
    kfree(file);
}

int pal_file_exists(const char* filename)
{
    return fat_file_exists(filename);
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

/* ================= MATH (pal_math.h) ================= */
/* No libm under -nostdlib -- every one of these is a from-scratch
   numerical routine, not a libm wrapper. See this file's header comment
   for pal_pow()'s accuracy note. */

double pal_floor(double x)
{
    long long i = (long long)x; /* truncates toward zero */
    double truncated = (double)i;
    return (truncated > x) ? truncated - 1.0 : truncated;
}

double pal_fmod(double a, double b)
{
    double q = a / b;
    long long qi = (long long)q; /* truncate toward zero, matching fmod's sign-of-`a` convention */
    return a - (double)qi * b;
}

double pal_atof(const char* str)
{
    double result = 0.0;
    double sign = 1.0;
    double frac_scale = 0.1;
    int exp_sign = 1;
    int exp_val = 0;

    while (*str == ' ' || *str == '\t') str++;
    if (*str == '-') { sign = -1.0; str++; }
    else if (*str == '+') str++;

    while (*str >= '0' && *str <= '9')
        result = result * 10.0 + (double)(*str++ - '0');

    if (*str == '.')
    {
        str++;
        while (*str >= '0' && *str <= '9')
        {
            result += (double)(*str++ - '0') * frac_scale;
            frac_scale *= 0.1;
        }
    }

    if (*str == 'e' || *str == 'E')
    {
        str++;
        if (*str == '-') { exp_sign = -1; str++; }
        else if (*str == '+') str++;
        while (*str >= '0' && *str <= '9')
            exp_val = exp_val * 10 + (*str++ - '0');

        {
            int i;
            double p = 1.0;
            for (i = 0; i < exp_val; i++) p *= 10.0;
            result = (exp_sign > 0) ? result * p : result / p;
        }
    }

    return sign * result;
}

void pal_ftoa(double value, char* buffer, int precision)
{
    int neg = 0;
    long long int_part;
    double frac;
    int n = 0, i;
    char digits[24];
    int ndigits;

    if (value < 0) { neg = 1; value = -value; }

    /* round-half-up at the requested precision before splitting into
       integer/fractional parts, matching sprintf("%.*f")'s rounding
       rather than truncating the last digit */
    {
        double scale = 1.0;
        for (i = 0; i < precision; i++) scale *= 10.0;
        value += 0.5 / scale;
    }

    int_part = (long long)value;
    frac = value - (double)int_part;

    if (neg) buffer[n++] = '-';

    ndigits = 0;
    if (int_part == 0)
    {
        digits[ndigits++] = '0';
    }
    else
    {
        long long t = int_part;
        while (t > 0 && ndigits < (int)sizeof(digits))
        {
            digits[ndigits++] = (char)('0' + (int)(t % 10));
            t /= 10;
        }
    }
    for (i = ndigits - 1; i >= 0; i--)
        buffer[n++] = digits[i];

    if (precision > 0)
    {
        buffer[n++] = '.';
        for (i = 0; i < precision; i++)
        {
            long long d;
            frac *= 10.0;
            d = (long long)frac;
            if (d > 9) d = 9; /* guard rare FP rounding overshoot */
            buffer[n++] = (char)('0' + (int)d);
            frac -= (double)d;
        }
    }
    buffer[n] = '\0';
}

/* exp(x) via range reduction (x = k*ln2 + r, |r| <= ln2/2) then a Taylor
   series for exp(r), which converges fast since r is small. */
static double pal_exp_internal(double x)
{
    const double ln2 = 0.6931471805599453;
    int k, i;
    double r, term, sum;

    k = (int)(x / ln2 + (x >= 0.0 ? 0.5 : -0.5));
    r = x - (double)k * ln2;

    term = 1.0;
    sum = 1.0;
    for (i = 1; i <= 20; i++)
    {
        term *= r / (double)i;
        sum += term;
    }

    if (k >= 0)
        for (i = 0; i < k; i++) sum *= 2.0;
    else
        for (i = 0; i < -k; i++) sum *= 0.5;

    return sum;
}

/* ln(x), x > 0: extract x = m * 2^exp2 with m in [1,2) via the double's
   raw bit pattern, then ln(m) via 2*atanh((m-1)/(m+1)) -- a series that
   converges quickly precisely because m is already close to 1. */
static double pal_ln_internal(double x)
{
    union { double d; unsigned long long u; } bits;
    int exp2, i;
    double m, y, y2, term, sum;

    bits.d = x;
    exp2 = (int)((bits.u >> 52) & 0x7FF) - 1023;
    bits.u = (bits.u & 0x800FFFFFFFFFFFFFULL) | 0x3FF0000000000000ULL;
    m = bits.d;

    y = (m - 1.0) / (m + 1.0);
    y2 = y * y;
    term = y;
    sum = y;
    for (i = 1; i <= 15; i++)
    {
        term *= y2;
        sum += term / (double)(2 * i + 1);
    }

    return 2.0 * sum + (double)exp2 * 0.6931471805599453;
}

double pal_pow(double base, double exp)
{
    long long iexp;

    if (base == 0.0)
        return (exp == 0.0) ? 1.0 : 0.0;

    /* Exact fast path for whole-number exponents (the overwhelmingly
       common case from a calculator) via repeated squaring -- also the
       only path that handles a negative base, since ln() below requires
       base > 0. */
    iexp = (long long)exp;
    if ((double)iexp == exp)
    {
        int negexp = iexp < 0;
        double result = 1.0;
        double b = base;
        long long n = negexp ? -iexp : iexp;

        while (n > 0)
        {
            if (n & 1) result *= b;
            b *= b;
            n >>= 1;
        }
        return negexp ? 1.0 / result : result;
    }

    if (base < 0.0)
        return 0.0; /* fractional power of a negative number isn't real -- no complex support */

    return pal_exp_internal(exp * pal_ln_internal(base));
}

/* ================= OE INFO (pal_oe_info.h) ================= */

static void cpuid(unsigned int leaf, unsigned int* a, unsigned int* b, unsigned int* c, unsigned int* d)
{
    __asm__ volatile ("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf));
}

int pal_get_oe_info(pal_oe_info_t* info)
{
    unsigned int a, b, c, d;
    unsigned int max_ext;
    unsigned int brand[12];

    if (!info)
        return 0;

    info->cpu_cores = 1;   /* no SMP/multi-core bring-up yet -- this kernel only ever runs on one */
    info->cpu_threads = 1;
    pal_strcpy(info->architecture, "x86 (32-bit protected mode)");

    cpuid(0x80000000u, &a, &b, &c, &d);
    max_ext = a;
    if (max_ext >= 0x80000004u)
    {
        cpuid(0x80000002u, &brand[0], &brand[1], &brand[2], &brand[3]);
        cpuid(0x80000003u, &brand[4], &brand[5], &brand[6], &brand[7]);
        cpuid(0x80000004u, &brand[8], &brand[9], &brand[10], &brand[11]);
        pal_memcpy(info->cpu_name, brand, 48);
        info->cpu_name[48] = '\0';
        {
            int i = 47;
            while (i >= 0 && (info->cpu_name[i] == ' ' || info->cpu_name[i] == '\0'))
                info->cpu_name[i--] = '\0';
        }
    }
    else
    {
        pal_strcpy(info->cpu_name, "Unknown CPU (no extended CPUID brand string)");
    }

    info->cpu_mhz = 0; /* see this file's header comment */

    info->total_ram = (unsigned long long)pmm_get_highest_address();
    info->free_ram  = (unsigned long long)pmm_get_free_frame_count() * 4096ULL;

    info->backend_name = "OE Kernel PAL (bare metal)";

    info->disk_total = (unsigned long long)hal_disk_get_sector_count() * (unsigned long long)HAL_DISK_SECTOR_SIZE;
    info->disk_free  = fat_get_free_bytes();

    info->uptime_seconds = hal_timer_get_ticks() / HAL_TIMER_HZ;

    return 1;
}

/* ================= DIRECTORY & FILE COMMANDS (pal_dir_file_cmds.h) ================= */
/* Real subdirectory support, on top of fat.c's path-aware fat_*_in()/
   fat_resolve_path() API. current_dir/current_path track this backend's
   single working directory (there's no per-process state -- this OS is
   single-tasking, see the kernel roadmap notes). A leading '/' in any path
   is absolute-from-root; anything else resolves relative to current_dir. */

/* Splits "A/B/C" into dirbuf="A/B" (the part to resolve as a directory) and
   namebuf="C" (a single 8.3 name inside it). No slash at all means
   dirbuf="" (meaning "current_dir itself"). A leading slash is preserved in
   dirbuf so fat_resolve_path() still treats it as absolute. Returns 0 on a
   trailing slash with nothing after it (e.g. "A/") -- ambiguous, rejected
   rather than guessed at. */
static int split_path(const char* path, char* dirbuf, int dirbuf_size, char* namebuf, int namebuf_size)
{
    int len = (int)pal_strlen(path);
    int last_slash = -1;
    int i;

    for (i = 0; i < len; i++)
        if (path[i] == '/')
            last_slash = i;

    if (last_slash < 0)
    {
        if (len >= namebuf_size || dirbuf_size < 1)
            return 0;
        pal_strcpy(namebuf, path);
        dirbuf[0] = '\0';
        return 1;
    }

    if (last_slash >= dirbuf_size)
        return 0;
    for (i = 0; i < last_slash; i++)
        dirbuf[i] = path[i];
    dirbuf[last_slash] = '\0';
    if (last_slash == 0)
    {
        dirbuf[0] = '/';
        dirbuf[1] = '\0';
    }

    {
        int namelen = len - last_slash - 1;
        if (namelen <= 0 || namelen >= namebuf_size)
            return 0; /* trailing slash with nothing after -- ambiguous */
        for (i = 0; i < namelen; i++)
            namebuf[i] = path[last_slash + 1 + i];
        namebuf[namelen] = '\0';
    }
    return 1;
}

/* Resolves `path`'s directory part relative to current_dir -- the common
   first step of nearly every command below. */
static int resolve_dir_part(const char* dirpart, struct fat_dir* out)
{
    const char* p = (dirpart[0] == '\0') ? "." : dirpart;
    return fat_resolve_path(&current_dir, p, out);
}

static void path_push(const char* comp)
{
    int len = (int)pal_strlen(current_path);
    int i = 0;

    if (pal_strcmp(current_path, "/") != 0)
    {
        current_path[len] = '/';
        len++;
    }
    while (comp[i] != '\0' && len + i < (int)sizeof(current_path) - 1)
    {
        current_path[len + i] = comp[i];
        i++;
    }
    current_path[len + i] = '\0';
}

static void path_pop(void)
{
    int len = (int)pal_strlen(current_path);
    int i;

    if (pal_strcmp(current_path, "/") == 0)
        return;

    for (i = len - 1; i >= 0 && current_path[i] != '/'; i--)
        ;
    if (i <= 0)
    {
        current_path[0] = '/';
        current_path[1] = '\0';
    }
    else
    {
        current_path[i] = '\0';
    }
}

/* Replays the same component walk fat_resolve_path() just did successfully
   against current_path, so pal_pwd() has a real path string to report --
   FAT dirents carry no parent-path string, only cluster numbers, so this
   has to be tracked separately as cdr moves around. Only called after
   fat_resolve_path() already succeeded for this exact path. */
static void update_current_path(const char* path)
{
    int i = 0;

    if (path[0] == '/')
    {
        current_path[0] = '/';
        current_path[1] = '\0';
        i = 1;
    }

    while (path[i] != '\0')
    {
        char comp[64];
        int ci = 0;

        while (path[i] != '\0' && path[i] != '/' && ci < (int)sizeof(comp) - 1)
            comp[ci++] = path[i++];
        comp[ci] = '\0';
        if (path[i] == '/')
            i++;

        if (ci == 0 || pal_strcmp(comp, ".") == 0)
            continue;

        if (pal_strcmp(comp, "..") == 0)
            path_pop();
        else
            path_push(comp);
    }
}

int pal_mdr(const char* path)
{
    char dirpart[256], namepart[FAT_MAX_NAME];
    struct fat_dir target;

    if (!split_path(path, dirpart, sizeof(dirpart), namepart, sizeof(namepart)))
        return -1;
    if (!resolve_dir_part(dirpart, &target))
        return -1;
    return fat_mkdir_in(&target, namepart, 0) ? 0 : -1;
}

int pal_rdr(const char* path)
{
    char dirpart[256], namepart[FAT_MAX_NAME];
    struct fat_dir target;

    if (!split_path(path, dirpart, sizeof(dirpart), namepart, sizeof(namepart)))
        return -1;
    if (!resolve_dir_part(dirpart, &target))
        return -1;
    return fat_rmdir_in(&target, namepart) ? 0 : -1;
}

/* Copies every FILE at the top level of `src` into a freshly created `dst`
   directory. Nested subdirectories inside `src` are NOT walked/copied --
   scoped out to keep this a first cut; pal_mvdr() below builds on this same
   scope limit and will correctly refuse (via fat_rmdir_in()'s "must be
   empty" rule) to finish "moving" a source directory that has nested
   subdirectories, rather than silently losing them. */
int pal_cpdr(const char* src, const char* dst)
{
    struct fat_dir src_dir, dst_parent, dst_dir;
    char dst_dirpart[256], dst_namepart[FAT_MAX_NAME];
    struct fat_dirent entries[64];
    int count, i;

    if (!fat_resolve_path(&current_dir, src, &src_dir))
        return -1;
    if (!split_path(dst, dst_dirpart, sizeof(dst_dirpart), dst_namepart, sizeof(dst_namepart)))
        return -1;
    if (!resolve_dir_part(dst_dirpart, &dst_parent))
        return -1;
    if (!fat_mkdir_in(&dst_parent, dst_namepart, &dst_dir))
        return -1;

    count = fat_list_dir(&src_dir, entries, 64);
    for (i = 0; i < count; i++)
    {
        int size;
        unsigned char* buf;

        if (entries[i].attributes & FAT_ATTR_DIRECTORY)
            continue; /* nested subdirectory -- not copied, see this function's header note */
        if (pal_strcmp(entries[i].name, ".") == 0 || pal_strcmp(entries[i].name, "..") == 0)
            continue;

        size = fat_get_file_size_in(&src_dir, entries[i].name);
        if (size < 0)
            continue;

        buf = (unsigned char*)pal_alloc(size > 0 ? (unsigned int)size : 1);
        if (!buf)
            continue;

        if (size == 0 || fat_read_file_in(&src_dir, entries[i].name, buf, (unsigned int)size) == size)
            fat_write_file_in(&dst_dir, entries[i].name, buf, (unsigned int)size);
        pal_free(buf);
    }
    return 0;
}

/* A "move" is copy-then-delete-the-originals, exactly like pal_mvf() below
   does for a single file. Deletes only the files pal_cpdr() actually
   copied (top-level, non-recursive), then removes `src` itself -- which
   fat_rmdir_in() will correctly refuse if `src` still contains nested
   subdirectories this scope never touched, rather than silently dropping
   them. */
int pal_mvdr(const char* src, const char* dst)
{
    struct fat_dir src_dir;
    char src_dirpart[256], src_namepart[FAT_MAX_NAME];
    struct fat_dir src_parent;
    struct fat_dirent entries[64];
    int count, i;

    if (pal_cpdr(src, dst) != 0)
        return -1;
    if (!fat_resolve_path(&current_dir, src, &src_dir))
        return -1;

    count = fat_list_dir(&src_dir, entries, 64);
    for (i = 0; i < count; i++)
    {
        if (entries[i].attributes & FAT_ATTR_DIRECTORY)
            continue;
        if (pal_strcmp(entries[i].name, ".") == 0 || pal_strcmp(entries[i].name, "..") == 0)
            continue;
        fat_delete_file_in(&src_dir, entries[i].name);
    }

    if (!split_path(src, src_dirpart, sizeof(src_dirpart), src_namepart, sizeof(src_namepart)))
        return -1;
    if (!resolve_dir_part(src_dirpart, &src_parent))
        return -1;
    return fat_rmdir_in(&src_parent, src_namepart) ? 0 : -1;
}

int pal_rnmdr(const char* oldname, const char* newname)
{
    return pal_mvdr(oldname, newname);
}

int pal_cdr(const char* path)
{
    struct fat_dir target;

    if (!fat_resolve_path(&current_dir, path, &target))
        return -1;
    current_dir = target;
    update_current_path(path);
    return 0;
}

int pal_pwd(char* buffer, int size)
{
    int len = (int)pal_strlen(current_path);

    if (len + 1 > size)
        return -1;
    pal_strcpy(buffer, current_path);
    return 0;
}

/* This used to just pal_println() every entry with no regard for the fixed
   80x25 screen -- on a real terminal (pal_windows.c/pal_linux.c) that's
   harmless scrollback, but this kernel's VGA screen is a fixed buffer with
   no scrollback: printing past the last row scrolls the WHOLE screen,
   including whatever bordered frame was already drawn above, permanently
   off. Fixed by adopting the exact same ui_init()+fixed-row+paginated
   pattern pal_windows.c/pal_linux.c's pal_ldr() already use (UI_LDR_*
   constants from ui_coordinates.h) instead of scrolling prints. */
int pal_ldr(const char* path)
{
    struct fat_dirent entries[64];
    struct fat_dir target;
    const char* p = (path[0] == '\0') ? "." : path;
    int count, i;
    int page_items = 0;
    int row;

    if (!fat_resolve_path(&current_dir, p, &target))
        return -1;

    count = fat_list_dir(&target, entries, 64);

    ui_init();

    pal_set_cursor(UI_LDR_HEADER_ROW, UI_LDR_HEADER_COL);
    pal_print(yellow bold " Directory of: ");
    pal_print(cyan bold);
    pal_print(path[0] == '\0' ? current_path : path);
    pal_print(reset);

    pal_set_cursor(UI_LDR_COLHEAD_ROW, UI_LDR_COLHEAD_COL);
    pal_print(white bold "NAME");
    pal_set_cursor(UI_LDR_COLHEAD_ROW, UI_LDR_COLHEAD_COL + UI_LDR_WIDTH - 10);
    pal_print(white bold "SIZE(bytes)");

    pal_set_cursor(UI_LDR_SEP_ROW, UI_LDR_SEP_COL);
    pal_print(blue bold "----------------------------------------------------------" reset);

    row = UI_LDR_FIRST_ROW;

    for (i = 0; i < count; i++)
    {
        char line[UI_LDR_WIDTH + 1];
        char sizebuf[16];
        int n = 0, j;

        for (j = 0; entries[i].name[j] != '\0' && n < UI_LDR_WIDTH - 12; j++)
            line[n++] = entries[i].name[j];
        while (n < UI_LDR_WIDTH - 12)
            line[n++] = ' ';
        pal_itoa((int)entries[i].size, sizebuf);
        for (j = 0; sizebuf[j] != '\0' && n < UI_LDR_WIDTH; j++)
            line[n++] = sizebuf[j];
        line[n] = '\0';

        pal_set_cursor(row, UI_LDR_COL);
        pal_print(line);

        page_items++;
        row++;

        if (page_items >= UI_LDR_PAGE_SIZE && i + 1 < count)
        {
            int r, c;

            pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
            pal_print(yellow bold "[Press any key for next page]" reset);
            pal_pause();

            for (r = UI_LDR_FIRST_ROW; r < UI_LDR_FIRST_ROW + UI_LDR_PAGE_SIZE; r++)
            {
                pal_set_cursor(r, UI_LDR_COL);
                for (c = 0; c < UI_LDR_WIDTH; c++)
                    pal_putchar(' ');
            }
            row = UI_LDR_FIRST_ROW;
            page_items = 0;
        }
    }

    pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
    pal_print(yellow bold "[End of directory - Press any key to return]" reset);
    pal_pause();
    return 0;
}

int pal_rmf(const char* file)
{
    char dirpart[256], namepart[FAT_MAX_NAME];
    struct fat_dir target;

    if (!split_path(file, dirpart, sizeof(dirpart), namepart, sizeof(namepart)))
        return -1;
    if (!resolve_dir_part(dirpart, &target))
        return -1;
    return fat_delete_file_in(&target, namepart) ? 0 : -1;
}

int pal_rdf(const char* file)
{
    char dirpart[256], namepart[FAT_MAX_NAME];
    struct fat_dir target;
    int size;
    unsigned char* buf;

    if (!split_path(file, dirpart, sizeof(dirpart), namepart, sizeof(namepart)))
        return -1;
    if (!resolve_dir_part(dirpart, &target))
        return -1;

    size = fat_get_file_size_in(&target, namepart);
    if (size < 0)
        return -1;

    buf = (unsigned char*)pal_alloc((unsigned int)size + 1);
    if (!buf)
        return -1;

    if (size > 0 && fat_read_file_in(&target, namepart, buf, (unsigned int)size) != size)
    {
        pal_free(buf);
        return -1;
    }

    buf[size] = '\0';

    /* Same fixed-buffer-screen problem as pal_ldr() above: a file longer
       than one screenful used to be dumped with a single unbounded
       pal_print(), scrolling the whole VGA screen (border included) off
       for anything past ~18 lines. pal_windows.c/pal_linux.c don't need
       this (real terminal scrollback), but this backend does -- paginate
       into the same fixed content region pal_ldr() uses, wrapping long
       lines at the frame's content width instead of letting them run past
       the right-hand border column too. */
    {
        int row = UI_LDR_FIRST_ROW;
        int col = 0;
        int i;

        ui_init();
        pal_set_cursor(UI_LDR_HEADER_ROW, UI_LDR_HEADER_COL);
        pal_print(yellow bold " Viewing: ");
        pal_print(cyan bold);
        pal_print(file);
        pal_print(reset);
        pal_set_cursor(UI_LDR_SEP_ROW, UI_LDR_SEP_COL);
        pal_print(blue bold "----------------------------------------------------------" reset);

        pal_set_cursor(row, UI_LDR_COL);

        for (i = 0; i < size; i++)
        {
            char ch = (char)buf[i];
            int newline = (ch == '\n');

            if (!newline)
            {
                pal_putchar(ch);
                col++;
            }

            if (newline || col >= UI_LDR_WIDTH)
            {
                row++;
                col = 0;

                if (row >= UI_LDR_FIRST_ROW + UI_LDR_PAGE_SIZE && i + 1 < size)
                {
                    int r, c;

                    pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
                    pal_print(yellow bold "[Press any key for next page]" reset);
                    pal_pause();

                    for (r = UI_LDR_FIRST_ROW; r < UI_LDR_FIRST_ROW + UI_LDR_PAGE_SIZE; r++)
                    {
                        pal_set_cursor(r, UI_LDR_COL);
                        for (c = 0; c < UI_LDR_WIDTH; c++)
                            pal_putchar(' ');
                    }
                    row = UI_LDR_FIRST_ROW;
                }

                pal_set_cursor(row, UI_LDR_COL);
            }
        }

        pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
        pal_print(yellow bold "[End of file - Press any key to return]" reset);
        pal_pause();
    }

    pal_free(buf);
    return 0;
}

int pal_cpf(const char* src, const char* dst)
{
    char sdir[256], sname[FAT_MAX_NAME];
    char ddir[256], dname[FAT_MAX_NAME];
    struct fat_dir sdirh, ddirh;
    int size, written;
    unsigned char* buf;

    if (!split_path(src, sdir, sizeof(sdir), sname, sizeof(sname)))
        return -1;
    if (!split_path(dst, ddir, sizeof(ddir), dname, sizeof(dname)))
        return -1;
    if (!resolve_dir_part(sdir, &sdirh))
        return -1;
    if (!resolve_dir_part(ddir, &ddirh))
        return -1;

    size = fat_get_file_size_in(&sdirh, sname);
    if (size < 0)
        return -1;

    buf = (unsigned char*)pal_alloc(size > 0 ? (unsigned int)size : 1);
    if (!buf)
        return -1;

    if (size > 0 && fat_read_file_in(&sdirh, sname, buf, (unsigned int)size) != size)
    {
        pal_free(buf);
        return -1;
    }

    written = fat_write_file_in(&ddirh, dname, buf, (unsigned int)size);
    pal_free(buf);
    return (written == size) ? 0 : -1;
}

int pal_mvf(const char* src, const char* dst)
{
    char dirpart[256], namepart[FAT_MAX_NAME];
    struct fat_dir target;

    if (pal_cpf(src, dst) != 0)
        return -1;
    if (!split_path(src, dirpart, sizeof(dirpart), namepart, sizeof(namepart)))
        return -1;
    if (!resolve_dir_part(dirpart, &target))
        return -1;
    return fat_delete_file_in(&target, namepart) ? 0 : -1;
}

int pal_rnmf(const char* oldname, const char* newname)
{
    return pal_mvf(oldname, newname);
}
