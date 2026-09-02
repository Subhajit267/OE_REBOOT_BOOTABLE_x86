/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-23
Module: Utilities
File: utils.h
About: Helper utilities – added separate echo/noecho input.
Revisions:
- 2026-02-21  Initial implementation
- 2026-02-21  Converted to PAL-only backend
- 2026-02-23  Added util_get_string_echo for normal input
------------------------------------------------------------
*/

#ifndef UTILS_H
#define UTILS_H

#include "pal.h"

/* ================= INPUT ================= */
bool util_get_int(int* value, const char* prompt, int min, int max);
bool util_get_double(double* value, const char* prompt);
char util_get_char(const char* prompt);
//void util_get_string(char* buffer, int max_len, const char* prompt);      /* for passwords (no echo) */
//void util_get_string_echo(char* buffer, int max_len, const char* prompt); /* for normal input (with echo) */
bool parse_double(const char* str, double* result); ///parseer
/* ================= STRING TRIMMING ================= */
void trim_whitespace(char* str);

/* ================= TIMER ================= */
void util_timer(int seconds, int row, int column);

/* ================= ACTIVATION ================= */
bool util_verify_key_ttt(const char* key);
bool util_verify_key_quiz(const char* key);

/* ================= SYSTEM LOCK DOWN ================= */
void lockdown(char* string);

#endif