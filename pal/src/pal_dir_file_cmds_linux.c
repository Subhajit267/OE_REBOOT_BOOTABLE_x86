/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-05
Date Last Modified: 2026-03-10
Module: PAL
File: pal_dir_file_cmds_linux.c
About: Linux implementation of PAL filesystem commands.
       Implements directory and file operations using
       POSIX filesystem APIs.
Revisions:
- 2026-03-05  Initial creation
- 2026-03-10  BUG FIX: pal(reset) typo corrected to pal_print(reset)
- 2026-08-25  BUG FIX: pal_ldr() built a per-entry fullpath[512] from
              p + "/" + dir->d_name with no length check — a long
              enough directory entry could overflow it. Added a
              bounds check that skips (not crashes on) an entry
              whose full path wouldn't fit.
- 2026-08-25  SECURITY: pal_cpdr() shelled out to "cp -r" with
              unsanitized src/dst interpolated into a quoted command
              string; an embedded '"' could break out of the
              quoting. Reject paths containing '"' and check the
              system() exit status instead of passing it straight
              through.
------------------------------------------------------------
*/
#include "pal_dir_file_cmds.h"
#ifdef OE_FILE_DIR_CMDS_LINUX
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>


/*
* -------- DIRECTORY COMMANDS --------
*/


int pal_mdr(const char* path)
{
    return mkdir(path, 0755);
}


int pal_rdr(const char* path)
{
    return rmdir(path);
}


int pal_cdr(const char* path)
{
    return chdir(path);
}

int pal_ldr(const char* path)
{
    DIR* d;
    struct dirent* dir;
    struct stat st;
    char fullpath[512];
    const char* p = path ? path : ".";

    d = opendir(p);
    if (!d)
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

    while ((dir = readdir(d)) != NULL)
    {
        if (pal_strcmp(dir->d_name, ".") == 0 || pal_strcmp(dir->d_name, "..") == 0)
            continue;

        /* fullpath must hold p + "/" + d_name + '\0' — skip entries that
           wouldn't fit instead of overflowing the fixed buffer. */
        if (pal_strlen(p) + 1 + pal_strlen(dir->d_name) >= sizeof(fullpath))
            continue;

        pal_strcpy(fullpath, p);
        pal_strcat(fullpath, "/");
        pal_strcat(fullpath, dir->d_name);
        stat(fullpath, &st);

        char line[80];
        char sizebuf[16];
        int pos = 0;

        if (S_ISDIR(st.st_mode))
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
        int name_len = (int)pal_strlen(dir->d_name);
        if (name_len > max_name_len)
            name_len = max_name_len;

        pal_strncpy(line + pos, dir->d_name, name_len + 1);
        pos += name_len;

        while (pos < UI_LDR_WIDTH - 12)
            line[pos++] = ' ';

        if (!S_ISDIR(st.st_mode))
        {
            pal_itoa((int)st.st_size, sizebuf);
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

            /* Clear listing area */
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
    }

    closedir(d);

    pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
    pal_print(yellow bold "[End of directory - Press any key to return]" reset);
    pal_pause();

    return 0;
}
int pal_rnmdr(const char* oldname, const char* newname)
{
    return rename(oldname, newname);
}


int pal_mvdr(const char* src, const char* dst)
{
    return rename(src, dst);
}


int pal_cpdr(const char* src, const char* dst)
{
    char cmd[512];

    /* A '"' in src/dst would break out of the quoted arguments below
       and let arbitrary text reach the shell — reject it. */
    if (pal_strchr(src, '"') || pal_strchr(dst, '"'))
        return -1;

    snprintf(cmd, sizeof(cmd), "cp -r \"%s\" \"%s\"", src, dst);

    return system(cmd) == 0 ? 0 : -1;
}


/*
* -------- FILE COMMANDS --------
*/


int pal_rmf(const char* file)
{
    return remove(file);
}


int pal_rnmf(const char* oldname, const char* newname)
{
    return rename(oldname, newname);
}


int pal_mvf(const char* src, const char* dst)
{
    return rename(src, dst);
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
    FILE* fs = fopen(src, "rb");
    FILE* fd = fopen(dst, "wb");

    char buffer[4096];
    size_t bytes;

    if (!fs)
        return -1;

    if (!fd)
    {
        fclose(fs);
        return -1;
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), fs)) > 0)
        fwrite(buffer, 1, bytes, fd);

    fclose(fs);
    fclose(fd);

    return 0;
}
int pal_pwd(char* buffer, int size)
{
    if (getcwd(buffer, size) == NULL)
        return -1;

    return 0;
}
#endif