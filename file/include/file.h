/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-21
Module: File Abstraction
File: file.h
About: Binary file abstraction layer for OE_REBOOT.
       All system state is stored in binary format.
       Uses PAL file backend internally.
Revisions:
- 2026-02-21  Initial implementation
- 2026-02-21  Added safe default integer reader
- 2026-02-21  Added string read/write helpers
- 2026-02-21  Converted to PAL-based backend
------------------------------------------------------------
*/

#ifndef FILE_UTILS_H
#define FILE_UTILS_H

/* ================= FILE CHECK ================= */

/*
Returns:
    1 if file exists
    0 otherwise
*/
int file_exists(const char* filename);


/* ================= INTEGER STORAGE ================= */

/*
Reads integer from binary file.
Returns 0 if file missing or corrupted.
*/
int file_read_int(const char* filename);

/*
Reads integer but returns default_value
if file missing or invalid.
*/
int file_read_int_default(const char* filename, int default_value);

/*
Writes integer in binary mode (overwrite).
*/
void file_write_int(const char* filename, int value);


/* ================= STRING STORAGE ================= */

/*
Reads null-terminated string from binary file.

Parameters:
    filename
    buffer
    max_len

Returns:
    number of bytes read
    0 on failure

Guarantees:
    buffer is always null-terminated
*/
int file_read_string(const char* filename, char* buffer, int max_len);

/*
Writes null-terminated string
(including '\0') to binary file.
*/
void file_write_string(const char* filename, const char* str);

#endif