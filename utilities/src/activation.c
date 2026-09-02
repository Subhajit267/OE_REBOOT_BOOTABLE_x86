/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: Utilities
File: activation.c
About: Product key verification logic for OE applications.
Revisions:
- 2026-02-21  Initial implementation
- 2026-02-21  Converted to PAL string backend

ALL CHECKED AND WORKING
------------------------------------------------------------
*/

#include "utils.h"

/*
------------------------------------------------------------
Function: util_verify_key_ttt
Purpose : Validates TicTacToe activation key.
Logic   : (key[6] + key[0]) == (key[9] + key[5])
Key must be exactly 10 characters long.
------------------------------------------------------------
*/
bool util_verify_key_ttt(const char* key)
{
    if (!key || pal_strlen(key) != 10)
        return false;

    return (key[6] + key[0]) == (key[9] + key[5]);
}

/*
------------------------------------------------------------
Function: util_verify_key_quiz
Purpose : Validates Quiz activation key.
Logic   : (key[7] + key[8]) == (key[2] + key[9])
Key must be exactly 10 characters long.
------------------------------------------------------------
*/
bool util_verify_key_quiz(const char* key)
{
    if (!key || pal_strlen(key) != 10)
        return false;

    return (key[7] + key[8]) == (key[2] + key[9]);
}