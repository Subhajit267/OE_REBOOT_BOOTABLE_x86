/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-23
Module: File Abstraction
File: file.c
About: Implements binary file abstraction using PAL.
       Now trims trailing newline/carriage return when reading strings
       for compatibility with old text files.
Revisions:
- 2026-02-21  Initial implementation
- 2026-02-21  Migrated to binary storage
- 2026-02-21  Converted to PAL backend
- 2026-02-23  Added trimming of trailing \n and \r in file_read_string

ALL CHECKED AND WORKING
------------------------------------------------------------
*/

#include "file.h"
#include "pal.h"

/* ================= FILE CHECK ================= */

int file_exists(const char* filename)
{
    return pal_file_exists(filename);
}

/* ================= INTEGER STORAGE ================= */

int file_read_int(const char* filename)
{
    pal_file_t* f = pal_file_open_read(filename);
    int value = 0;

    if (!f)
        return 0;

    if (pal_file_read(f, &value, sizeof(int)) != sizeof(int))
        value = 0;

    pal_file_close(f);
    return value;
}

int file_read_int_default(const char* filename, int default_value)
{
    pal_file_t* f = pal_file_open_read(filename);
    int value;

    if (!f)
        return default_value;

    if (pal_file_read(f, &value, sizeof(int)) != sizeof(int))
    {
        pal_file_close(f);
        return default_value;
    }

    pal_file_close(f);
    return value;
}

void file_write_int(const char* filename, int value)
{
    pal_file_t* f = pal_file_open_write(filename);

    if (!f)
        return;

    pal_file_write(f, &value, sizeof(int));
    pal_file_close(f);
}

/* ================= STRING STORAGE ================= */

int file_read_string(const char* filename, char* buffer, int max_len)
{
    pal_file_t* f = pal_file_open_read(filename);
    int bytes_read;

    if (!f)
    {
        buffer[0] = '\0';
        return 0;
    }

    bytes_read = pal_file_read(f, buffer, max_len - 1);

    if (bytes_read < 0)
        bytes_read = 0;

    buffer[bytes_read] = '\0';

    /* Trim trailing newline or carriage return (for compatibility with old text files) */
    int len = bytes_read;
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
    {
        buffer[len - 1] = '\0';
        len--;
    }

    pal_file_close(f);
    return bytes_read;
}

void file_write_string(const char* filename, const char* str)
{
    pal_file_t* f = pal_file_open_write(filename);

    if (!f)
        return;

    pal_file_write(f, str, pal_strlen(str) + 1);
    pal_file_close(f);
}