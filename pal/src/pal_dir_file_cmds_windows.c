/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-05
Date Last Modified: 2026-03-05
Module: PAL
File: pal_file_cmds_windows.c
About: Windows implementation of PAL filesystem commands.
       Uses Win32 filesystem APIs to provide directory
       and file operations.

Revisions:
- 2026-03-05  Initial creation
- 2026-08-25  BUG FIX: pal_ldr() copied an unbounded caller-supplied
              path into a fixed char search[260] with no length
              check. Added a bounds check.
- 2026-08-25  SECURITY: pal_cpdr() shelled out to xcopy with
              unsanitized src/dst interpolated into a quoted command
              string; an embedded '"' could break out of the
              quoting. Reject paths containing '"' and check the
              system() exit status instead of passing it straight
              through.
------------------------------------------------------------
*/

#include "pal_dir_file_cmds.h"
#ifdef OE_FILE_DIR_CMDS_WINDOWS
#include <stdio.h>
#include <windows.h>
#include <string.h>


/*
* -------- DIRECTORY COMMANDS --------
*/


int pal_mdr(const char* path)
{
    return CreateDirectory(path, NULL) ? 0 : -1;
}


int pal_rdr(const char* path)
{
    return RemoveDirectory(path) ? 0 : -1;
}


int pal_cdr(const char* path)
{
    return SetCurrentDirectory(path) ? 0 : -1;
}

int pal_ldr(const char* path)
{
    WIN32_FIND_DATA fd;
    HANDLE h;
    char search[260];
    const char* p = path ? path : ".";

    /* search must hold p + "\*" + '\0' — reject anything that would overflow it */
    if (pal_strlen(p) > sizeof(search) - 3)
    {
        ui_status(STATUS_INVALID);
        return -1;
    }

    pal_strcpy(search, p);
    pal_strcat(search, "\\*");

    h = FindFirstFile(search, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return -1;

    int page_items = 0;
    int page_num = 1;

    ui_init();

    /* "Directory of:" line */
    pal_set_cursor(UI_LDR_HEADER_ROW, UI_LDR_HEADER_COL);
    pal_print(yellow bold " Directory of: ");
    pal_print(cyan bold);
    pal_print(p);
    pal_print(reset);

    /* Column headers */
    pal_set_cursor(UI_LDR_COLHEAD_ROW, UI_LDR_COLHEAD_COL);
    pal_print(white bold "NAME");
    pal_set_cursor(UI_LDR_COLHEAD_ROW, UI_LDR_COLHEAD_COL + UI_LDR_WIDTH - 10);
    pal_print(white bold "SIZE(kb)");

    /* Separator line */
    pal_set_cursor(UI_LDR_SEP_ROW, UI_LDR_SEP_COL);
    pal_print(blue bold "----------------------------------------------------------" reset);

    int row = UI_LDR_FIRST_ROW;

    do
    {
        if (pal_strcmp(fd.cFileName, ".") == 0 || pal_strcmp(fd.cFileName, "..") == 0)
            continue;

        char line[80];
        char sizebuf[16];
        int pos = 0;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            pal_strcpy(line, "[DIR]  ");
            pos = 6;
        }
        else
        {
            pal_strcpy(line, "[FILE] ");
            pos = 6;
        }

        int max_name_len = UI_LDR_WIDTH - pos - 12;
        int name_len = (int)pal_strlen(fd.cFileName);
        if (name_len > max_name_len)
            name_len = max_name_len;

        pal_strncpy(line + pos, fd.cFileName, name_len + 1);
        pos += name_len;

        while (pos < UI_LDR_WIDTH - 12)
            line[pos++] = ' ';

        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            pal_itoa(fd.nFileSizeLow, sizebuf);
            pal_strcpy(line + pos, sizebuf);
        }
        else
        {
            pal_strcpy(line + pos, "-");
        }
        line[UI_LDR_WIDTH] = '\0';

        pal_set_cursor(row, UI_LDR_COL);
        pal_print(line);

        page_items++;

        if (page_items >= UI_LDR_PAGE_SIZE)
        {
            pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
            pal_print(yellow bold "[Press any key for next page]" reset);
            page_num++;
            pal_pause();

            for (int r = UI_LDR_FIRST_ROW; r < UI_LDR_FIRST_ROW + UI_LDR_PAGE_SIZE; r++)
            {
                pal_set_cursor(r, UI_LDR_COL);
                for (int c = 0; c < UI_LDR_WIDTH; c++)
                    pal_putchar(' ');
            }
            row = UI_LDR_FIRST_ROW;
            page_items = 0;
        }
        else
        {
            row++;
        }

    } while (FindNextFile(h, &fd));

    FindClose(h);

    pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
    pal_print(yellow bold "[End of directory - Press any key to return]" reset);
    pal_pause();

    return 0;
}

int pal_rnmdr(const char* oldname, const char* newname)
{
    return MoveFile(oldname, newname) ? 0 : -1;
}


int pal_mvdr(const char* src, const char* dst)
{
    return MoveFile(src, dst) ? 0 : -1;
}


int pal_cpdr(const char* src, const char* dst)
{
    char cmd[512];

    /* A '"' in src/dst would break out of the quoted arguments below
       and let arbitrary text reach the shell — reject it. */
    if (pal_strchr(src, '"') || pal_strchr(dst, '"'))
        return -1;

    snprintf(cmd, sizeof(cmd), "xcopy \"%s\" \"%s\" /E /I /Y", src, dst);

    return system(cmd) == 0 ? 0 : -1;
}


/*
* -------- FILE COMMANDS --------
*/


int pal_rmf(const char* file)
{
    return DeleteFile(file) ? 0 : -1;
}


int pal_rnmf(const char* oldname, const char* newname)
{
    return MoveFile(oldname, newname) ? 0 : -1;
}


int pal_mvf(const char* src, const char* dst)
{
    return MoveFile(src, dst) ? 0 : -1;
}


int pal_rdf(const char* file)
{
    FILE* f = fopen(file, "r");
    int ch;

    if (!f)
        return -1;

    while ((ch = fgetc(f)) != EOF)
        putchar(ch);

    fclose(f);

    return 0;
}


int pal_cpf(const char* src, const char* dst)
{
    return CopyFile(src, dst, FALSE) ? 0 : -1;
}

int pal_pwd(char* buffer, int size)
{
    if (GetCurrentDirectory(size, buffer) == 0)
        return -1;

    return 0;
}

#endif  