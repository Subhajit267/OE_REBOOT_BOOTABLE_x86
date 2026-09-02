/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-23
Module: Utilities
File: input_validation.c
About: Advanced input validation with manual echo control.
Revisions:
- 2026-02-21  Initial implementation
- 2026-02-21  Converted to PAL backend
- 2026-02-23  Split into echo/noecho, removed terminal echo dependency

ALL CHECKED AND WORKING
------------------------------------------------------------
*/

#include "utils.h"
#include "ui_setup.h"
///* ================= INTERNAL LINE READER WITH MANUAL ECHO ================= */
//static void read_line(char* buffer, int max_len, int manual_echo)
//{
//    int i = 0;
//    char c;
//
//    while (i < max_len - 1)
//    {
//        c = pal_getchar();   /* raw input, no terminal echo */
//
//        if (c == '\n' || c == '\r')
//            break;
//
//        if (c == 8 || c == 127)   /* backspace */
//        {
//            if (i > 0)
//            {
//                i--;
//                if (manual_echo)
//                    pal_print("\b \b");
//            }
//        }
//        else
//        {
//            buffer[i++] = c;
//            if (manual_echo)
//                pal_putchar(c);
//        }
//    }
//
//    buffer[i] = '\0';
//    pal_print("\n");
//}

/* ================= INTEGER PARSER ================= */
static bool parse_int(const char* str, int* result)
{
    int i = 0;
    int sign = 1;
    int value = 0;

    if (str[0] == '\0')
        return false;

    if (str[i] == '-')
    {
        sign = -1;
        i++;
    }

    if (str[i] == '\0')
        return false;

    for (; str[i] != '\0'; i++)
    {
        if (str[i] < '0' || str[i] > '9')
            return false;

        value = value * 10 + (str[i] - '0');
    }

    *result = value * sign;
    return true;
}

/* ================= INTEGER INPUT ================= */
bool util_get_int(int* value, const char* prompt, int min, int max)
{
    char buffer[32];
    int num;

    while (true)
    {
        pal_print(prompt);
        pal_readline(buffer, sizeof(buffer));
        if (!parse_int(buffer, &num))
        {
			ui_status(STATUS_INVALID_NUMBER_DECIMAL_FORMAT);
            continue;
        }

        if (num < min || num > max)
        {
            ui_status(STATUS_VALUUE_OUT_OF_RANGE);
            continue;
        }

        *value = num;
        return true;
    }
}

/* ================= DOUBLE PARSER ================= */
bool parse_double(const char* str, double* result)
{
    int i = 0;
    int sign = 1;
    double value = 0;
    double fraction = 0;
    double divisor = 10;
    bool decimal_found = false;

    if (str[0] == '\0')
        return false;

    if (str[i] == '-')
    {
        sign = -1;
        i++;
    }

    for (; str[i] != '\0'; i++)
    {
        if (str[i] == '.')
        {
            if (decimal_found)
                return false;

            decimal_found = true;
            continue;
        }

        if (str[i] < '0' || str[i] > '9')
            return false;

        if (!decimal_found)
            value = value * 10 + (str[i] - '0');
        else
        {
            fraction += (str[i] - '0') / divisor;
            divisor *= 10;
        }
    }

    *result = (value + fraction) * sign;
    return true;
}

/* ================= DOUBLE INPUT ================= */
bool util_get_double(double* value, const char* prompt)
{
    char buffer[64];

    while (true)
    {
        pal_print(prompt);
        pal_readline(buffer, sizeof(buffer));

        if (!parse_double(buffer, value))
        {
			ui_status(STATUS_INVALID_NUMBER_DECIMAL_FORMAT);
            continue;
        }

        return true;
    }
}

/* ================= CHARACTER INPUT ================= */
char util_get_char(const char* prompt)
{
    char c;

    pal_print(prompt);
    c = pal_getchar();
    pal_putchar(c);
    pal_print("\n");

    /* Flush any remaining characters until newline */
    while (pal_getchar() != '\n')
        ;

    return c;
}

///* ================= STRING INPUT (NO ECHO) ================= */
//void util_get_string(char* buffer, int max_len, const char* prompt)
//{
//    pal_print(prompt);
//    read_line(buffer, max_len, 0);
//}
//
///* ================= STRING INPUT (WITH ECHO) ================= */
//void util_get_string_echo(char* buffer, int max_len, const char* prompt)
//{
//    pal_print(prompt);
//    read_line(buffer, max_len, 1);
//}

/* ================= STRING TRIMMING ================= */
void trim_whitespace(char* str)
{
    if (!str) return;

    int len = pal_strlen(str);
    int start = 0;
    int end = len - 1;

    // Trim leading whitespace
    while (start < len && (str[start] == ' ' || str[start] == '\t' || str[start] == '\n' || str[start] == '\r'))
        start++;

    // Trim trailing whitespace
    while (end >= start && (str[end] == ' ' || str[end] == '\t' || str[end] == '\n' || str[end] == '\r'))
        end--;

    // Shift the trimmed string to the beginning
    int i;
    for (i = 0; i <= end - start; i++)
        str[i] = str[start + i];
    str[i] = '\0';
}