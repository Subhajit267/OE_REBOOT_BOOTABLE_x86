/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-03
Date Last Modified: 2026-03-03
Module: PAL
File: pal_math.h
About: Platform abstraction layer – mathematical utilities.
       Provides portable math functions for OE system tools
       and applications.

Revisions:
- 2026-03-03  Initial creation
------------------------------------------------------------
*/

#ifndef PAL_MATH_H
#define PAL_MATH_H

double pal_pow(double base, double exp);
double pal_floor(double x);
double pal_fmod(double a, double b);
double pal_atof(const char* str);
void   pal_ftoa(double value, char* buffer, int precision);

#endif