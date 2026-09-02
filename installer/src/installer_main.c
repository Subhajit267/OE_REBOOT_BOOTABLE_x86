/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-31
Date Last Modified: 2026-09-01
Module: Installer
File: installer_main.c
About: The actual entry point of installer.bin -- a separate, minimal
       boot target from the real OS's kernel.bin, isolated per the
       project's disk-deploy-installer plan. Reuses boot/src/boot.s and
       kernel/linker.ld unchanged (GRUB loads this exactly like
       kernel.bin, calling the same `kernel_main` symbol -- here that
       symbol is this installer flow instead of the real OS).

       Links pal_kernel.c + ui_elements.c/bootscreen.c/ui_setup.c/file.c
       (see the Makefile's INSTALLER_OBJS comment for why that's not the
       dependency wall it looks like) so this screen uses the exact same
       bordered, colored look and the same bootscreen_show() loading
       screen as the real OS. Also links notepad and systeminfo (Try Mode
       uses both) -- confirmed by inspection to depend on nothing outside
       pal.h/ui_setup.h/branding.h plus, for systeminfo.c, two symbols
       (current_user/user_exists()) stubbed in below rather than pulling
       in the rest of user_management.

       Flow: bootscreen_show() -> detect the target disk -> a live-CD
       style menu, [1] Install or [2] Try (Guest Mode):
         - Install: destructive-action warning (typed "YES") -> write
           the Stage-4 boot chain + partition table (progress bar) ->
           format the OEFS/FAT32 partition (progress bar) -> reboot.
           The freshly-written disk then boots on its own via the
           Stage-4 bootloader with zero installer/GRUB/ISO involvement,
           straight into the real kernel_main(), which -- since no
           user.bd exists yet -- runs installer_prompt() as the
           OOBE-equivalent step. On bare metal that skips straight to
           user creation (no Y/N wizard -- see setup/src/installer.c's
           pal_is_bare_metal() check, since "install" was already decided
           by getting here at all): bootscreen -> "Preparing for first
           boot..." -> user creation -> improvements -> reboot.
         - Try (Guest Mode): non-destructively looks for an existing
           filesystem on the target disk (mbr_find_partition + fat_mount
           -- never formats), then drops into a small built-in shell --
           file/dir commands + notepad + sysinfo, scoped to what's
           actually useful before an OS is installed (no calculator/
           regedit/settings, those are real-OS "apps" this binary never
           links). Redrawn every command via ui_init() at the same
           UI_PROMPT_* / UI_STATUS_ROW coordinates system_core/prompt.c's
           real shell uses (Command-> prompt, ui_status() codes), so it
           looks and behaves like the same interface, not a separate
           plain-terminal one. `install` from inside Try Mode runs the
           exact same install routine as the main menu's [1]; `menu`
           returns to the main menu.
Revisions:
- 2026-08-31  Initial creation (Stage 5 of the FAT32/MBR/bootloader/
              installer roadmap): detect disk -> typed-YES confirm ->
              write boot chain + partition table -> format -> reboot.
              No menu -- install was the only path.
- 2026-09-01  Added the live-CD style [1] Install / [2] Try menu and the
              Try Mode guest shell (file/dir commands + notepad).
- 2026-09-01  Try Mode's `sysinfo` switched to the real oe_systeminfo_entry()
              (systeminfo.o linked in, current_user/user_exists() stubbed)
              instead of a custom summary; kernel_main() now calls
              pmm_init() for real so it reports real RAM numbers. Try
              Mode's whole shell redrawn to match system_core/prompt.c's
              real interface (UI_PROMPT_* / UI_STATUS_ROW, Command-> prompt,
              ui_status() codes) instead of a plain scrolling terminal.
              draw_header() switched from a shallow content-row clear to
              a full ui_init(), fixing stale text/a missing border when
              `install` was run from inside Try Mode.
------------------------------------------------------------
*/

#include "hal.h"
#include "mbr.h"
#include "fat.h"
#include "pmm.h"
#include "pal.h"
#include "pal_dir_file_cmds.h"
#include "ui_setup.h"
#include "ui_elements.h"
#include "bootscreen.h"
#include "notepad.h"
#include "systeminfo.h"
#include "user.h"
#include "installer_assets.h"

/* systeminfo.c (system_tools/systeminfo) is linked in so Try Mode's `sysinfo`
   is the exact same 3-page bordered screen the real OS shows -- confirmed by
   inspection to need only two symbols outside pal.h/ui_setup.h/branding.h
   (both header-only, no link cost): user.h's `current_user`/`user_exists()`.
   Rather than link the rest of user_management just to satisfy those two,
   they're defined here directly -- this binary has no login concept, so
   "no user, always GUEST" is the semantically correct answer for it, not a
   workaround. */
char current_user[USERNAME_MAX] = "";
int user_exists(void) { return 0; }

#define ROW_HEADER    4
#define ROW_DISK      6
#define ROW_MENU      9
#define ROW_WARNING   9
#define ROW_PROMPT   10
#define ROW_PLABEL   12
#define ROW_PROGRESS 13
#define ROW_STATUS   16
/* ui_init()'s logo() draws a boxed logo at rows 6-12, cols 4-20 (see
   ui_coordinates.h's UI_LOGO_ROW_START/UI_LOGO_COL_START) -- content must
   stay clear of that box, hence starting past col 20 rather than at the
   frame's own left margin. */
#define COL_CONTENT  22
#define PROGRESS_WIDTH 40

/* Must match the Makefile's BOOTDISK_KERNEL_LBA/BOOTDISK_PART_START (and the
   STAGE2_LBA the shared mbr.bin/stage2.bin/kernel.flat.bin recipe assembles
   stage2.bin with) so a disk this installer writes boots identically to one
   `make bootdisk.img` writes for dev testing. */
#define INSTALL_STAGE2_LBA      1
#define INSTALL_KERNEL_LBA      17
#define INSTALL_PART_START_LBA  2048

static unsigned int g_sector_count;
static char g_disk_line[64];

/* Full ui_init() (not just clear_content_rows()) so this is safe to call as
   the first thing on a freshly entered screen regardless of what was on
   screen before -- in particular, Try Mode's plain-scrolling shell (below)
   doesn't keep the bordered frame up, so `install` run from inside it needs
   this to redraw the frame from scratch rather than leaving stale text and
   a missing border behind. */
static void draw_header(void)
{
    ui_init();
    ui_title(ROW_HEADER, COL_CONTENT, cyan bold, "OE Reboot -- Disk Installer");
    ui_title(ROW_DISK, COL_CONTENT, white bold, g_disk_line);
}

/* Writes `len` bytes from `data` starting at `start_lba`, sector by sector,
   zero-padding the final partial sector -- stage2.bin's raw assembled size
   in particular isn't guaranteed to land on a 512-byte boundary. */
static int write_image(unsigned int start_lba, const unsigned char* data, unsigned int len)
{
    unsigned int sectors = (len + HAL_DISK_SECTOR_SIZE - 1) / HAL_DISK_SECTOR_SIZE;
    unsigned int s;

    for (s = 0; s < sectors; s++)
    {
        unsigned char sector[HAL_DISK_SECTOR_SIZE];
        unsigned int offset = s * HAL_DISK_SECTOR_SIZE;
        unsigned int chunk = len - offset;
        unsigned int i;

        if (chunk > HAL_DISK_SECTOR_SIZE)
            chunk = HAL_DISK_SECTOR_SIZE;

        for (i = 0; i < HAL_DISK_SECTOR_SIZE; i++)
            sector[i] = (i < chunk) ? data[offset + i] : 0;

        if (!hal_disk_write_sector(start_lba + s, sector))
            return 0;
    }
    return 1;
}

/* Fatal-error path: show it in the same styled frame, pause so it's
   readable, then really power off (pal_exit(), not a bare hlt loop) --
   matches how the real OS's own fatal-boot-error path behaves. */
static void fail(const char* message)
{
    ui_title(ROW_STATUS, COL_CONTENT, RED bold, message);
    pal_sleep(3);
    pal_exit();
}

/* ================= INSTALL ================= */

/* Destructive-warning + typed "YES" confirm, then writes the boot chain,
   partition table and OEFS/FAT32 filesystem onto the target disk, with a
   cosmetic progress bar under each phase label (same convention
   setup/src/installer.c already uses for its own "system initialization"
   step -- these bars are a fixed animation, not a byte-counted meter; the
   real disk work underneath is fast enough that a real meter would just
   flash by). Reboots into the freshly-written disk on success. Returns to
   the caller only if the user aborted the YES prompt -- shared by the main
   menu's [1] Install and Try Mode's `install` command, so neither of those
   callers should assume this never returns. */
static void do_install(void)
{
    unsigned int part_sectors;
    char answer[16];

    if (g_sector_count <= INSTALL_PART_START_LBA + 4096u)
        fail("Disk is too small to install onto.");

    draw_header();
    ui_title(ROW_WARNING, COL_CONTENT, RED bold, "WARNING: this will ERASE ALL DATA on this disk.");
    pal_set_cursor(ROW_PROMPT, COL_CONTENT);
    pal_print("Type YES (all caps) to continue, anything else to abort: ");
    pal_readline(answer, sizeof(answer));

    if (pal_strcmp(answer, "YES") != 0)
    {
        ui_title(ROW_STATUS, COL_CONTENT, yellow bold, "Aborted -- nothing was written.");
        pal_sleep(2);
        return;
    }

    part_sectors = g_sector_count - INSTALL_PART_START_LBA;

    draw_header();
    ui_title(ROW_PLABEL, COL_CONTENT, yellow bold, "Formatting disk with OEFS...");
    progressbar(ROW_PROGRESS, COL_CONTENT, PROGRESS_WIDTH);

    if (!write_image(0, installer_mbr_bin, installer_mbr_bin_len))
        fail("FATAL: failed writing the boot sector.");
    if (!write_image(INSTALL_STAGE2_LBA, installer_stage2_bin, installer_stage2_bin_len))
        fail("FATAL: failed writing the Stage 2 loader.");
    if (!write_image(INSTALL_KERNEL_LBA, installer_kernel_bin, installer_kernel_bin_len))
        fail("FATAL: failed writing the kernel image.");

    /* installer_mbr_bin above already wrote a full, valid 512-byte boot
       sector (bootstrap code + a zeroed partition table + 0x55AA), so
       preserve_bootstrap=1 here keeps that bootstrap code and only fills in
       the partition table entry describing the new OEFS partition. */
    if (!mbr_write(INSTALL_PART_START_LBA, part_sectors, MBR_TYPE_FAT32_LBA, 1))
        fail("FATAL: failed writing the partition table.");

    draw_header();
    ui_title(ROW_PLABEL, COL_CONTENT, yellow bold, "Copying system files...");
    progressbar(ROW_PROGRESS, COL_CONTENT, PROGRESS_WIDTH);

    if (!fat_format_at(INSTALL_PART_START_LBA, part_sectors))
        fail("FATAL: failed formatting the OEFS partition.");

    ui_title(ROW_STATUS, COL_CONTENT, green bold, "Installation complete.");
    ui_title(ROW_STATUS + 1, COL_CONTENT, white bold, "Remove the installation media, then press any key to reboot.");

    pal_getchar();
    pal_reboot(); /* real warm reboot (hal_reboot(), 8042 pulse) -- boots straight into the
                     freshly-written disk's own Stage-4 bootloader as long as the media was
                     actually removed/the boot order falls through to the disk, exactly like
                     any real installer's "reboot to finish" step. */
}

/* ================= TRY MODE (GUEST SHELL) ================= */

static void str_lower(char* s)
{
    while (*s)
    {
        if (*s >= 'A' && *s <= 'Z')
            *s = (char)(*s - 'A' + 'a');
        s++;
    }
}

/* Same split as system_core/prompt.c's parse_command()/parse_two_args() --
   duplicated here (rather than exposed from prompt.c) since installer.bin
   never links system_core, and this is genuinely all this shell needs. */
static void split_cmd(char* input, char* cmd, int cmd_max, char* args, int args_max)
{
    int i = 0, j = 0;

    while (input[i] != ' ' && input[i] != '\0')
    {
        if (j < cmd_max - 1) cmd[j++] = input[i];
        i++;
    }
    cmd[j] = '\0';

    if (input[i] == ' ') i++;

    j = 0;
    while (input[i] != '\0')
    {
        if (j < args_max - 1) args[j++] = input[i];
        i++;
    }
    args[j] = '\0';
}

static void split_two(char* args, char* a, int a_max, char* b, int b_max)
{
    int i = 0, j = 0, k = 0;

    while (args[i] != ' ' && args[i] != '\0')
    {
        if (j < a_max - 1) a[j++] = args[i];
        i++;
    }
    a[j] = '\0';

    if (args[i] == ' ') i++;

    while (args[i] != '\0')
    {
        if (k < b_max - 1) b[k++] = args[i];
        i++;
    }
    b[k] = '\0';
}

/* Multi-line command reference -- its own full-screen page (ui_init() +
   pal_pause()), same convention pal_ldr()/oe_systeminfo_entry() use below,
   since it doesn't fit in the single-line UI_PROMPT_OUT_ROW slot the rest
   of this shell's results use. Rows 11-22 are the safe content band (below
   the logo, above UI_STATUS_ROW's own "press any key" line). */
static void try_help(int mounted)
{
    int row = UI_PROMPT_OUT_ROW;
    int col = UI_PROMPT_OUT_COL;

    ui_init();
    ui_title(UI_PROMPT_VER_ROW, UI_PROMPT_VER_COL, RED bold underline, "OE Reboot -- Try Mode Commands");

    ui_title(row++, col, white bold, "sysinfo   install   reboot / exit   menu");
    if (mounted)
    {
        ui_title(row++, col, white bold, "ldr [path]            list a directory");
        ui_title(row++, col, white bold, "cdr <path>            change directory");
        ui_title(row++, col, white bold, "pwd                   print working directory");
        ui_title(row++, col, white bold, "mdr/rdr <dir>         make/remove directory");
        ui_title(row++, col, white bold, "cpdr/mvdr <src> <dst> copy/move directory");
        ui_title(row++, col, white bold, "rnmdr <old> <new>     rename directory");
        ui_title(row++, col, white bold, "rdf/rmf <file>        view/delete file");
        ui_title(row++, col, white bold, "cpf/mvf <src> <dst>   copy/move file");
        ui_title(row++, col, white bold, "notepad [file]        text editor");
    }
    else
        ui_title(row++, col, yellow bold, "(file/dir commands + notepad need INSTALL first)");

    pal_set_cursor(UI_STATUS_ROW, UI_STATUS_COL);
    pal_print(yellow bold "[Press any key to return]" reset);
    pal_pause();
}

/* Non-destructively looks for an existing filesystem (never formats), then
   runs a small command loop scoped to file/dir ops + notepad + `install` --
   deliberately not the real hosted prompt()/cmd_table (this binary links
   none of system_core/user_management/system_tools -- current_user/
   user_exists() above are stubbed in just for systeminfo.c's sake -- and
   doesn't need the calculator/regedit/settings "apps" those own, only plain
   file handling). Redraws via ui_init() and uses the same UI_PROMPT_* /
   UI_STATUS_ROW coordinates and ui_status() codes system_core/prompt.c's
   real shell does, so this looks and behaves like the same interface, not
   a different plain-terminal one. Returns when the user types `menu`. */
static void try_mode(void)
{
    unsigned int part_lba, part_sectors;
    int have_partition, mounted;
    char cmdline[128], cmd[32], args[96];

    have_partition = mbr_find_partition(&part_lba, &part_sectors);
    mounted = have_partition ? fat_mount_at(part_lba) : fat_mount();
    if (mounted)
        pal_init();

    for (;;)
    {
        ui_init();
        ui_title(UI_PROMPT_VER_ROW, UI_PROMPT_VER_COL, RED bold underline, "OE Reboot -- Try Mode (Guest)");
        ui_title(UI_PROMPT_MSG_ROW, UI_PROMPT_MSG_COL, mounted ? green bold : yellow bold,
            mounted ? "Filesystem mounted -- type HELP for commands."
                    : "No filesystem yet -- type INSTALL, or HELP for commands.");
        ui_title(UI_PROMPT_CMD_ROW, UI_PROMPT_CMD_COL, blue bold, "Command->");

        pal_set_cursor(UI_PROMPT_CMD_ROW, UI_PROMPT_INPUT_COL);
        pal_readline(cmdline, sizeof(cmdline));
        split_cmd(cmdline, cmd, sizeof(cmd), args, sizeof(args));
        str_lower(cmd);

        if (pal_strlen(cmd) == 0)
            continue;
        else if (pal_strcmp(cmd, "help") == 0)
            try_help(mounted);
        else if (pal_strcmp(cmd, "menu") == 0)
            return;
        else if (pal_strcmp(cmd, "install") == 0)
            do_install(); /* reboots on success; only returns here if aborted */
        else if (pal_strcmp(cmd, "reboot") == 0)
            pal_reboot();
        else if (pal_strcmp(cmd, "exit") == 0 || pal_strcmp(cmd, "shutdown") == 0)
            pal_exit();
        else if (pal_strcmp(cmd, "sysinfo") == 0)
            oe_systeminfo_entry(); /* same 3-page bordered screen the real OS's `systeminfo` shows */
        else if (!mounted)
            ui_status(STATUS_NOT_INSTALLED);
        else if (pal_strcmp(cmd, "ldr") == 0)
            pal_ldr(args); /* draws its own full frame + pauses, same as the real OS's cmd_ldr */
        else if (pal_strcmp(cmd, "cdr") == 0)
        {
            if (pal_strlen(args) == 0) ui_status(STATUS_INVALID);
            else if (pal_cdr(args) != 0) ui_status(STATUS_ERROR);
        }
        else if (pal_strcmp(cmd, "pwd") == 0)
        {
            char path[256];
            if (pal_pwd(path, sizeof(path)) == 0)
            {
                /* col 19 leaves 61 usable columns before the col-79 edge,
                   same bound cmd_pwd() uses in system_core/prompt.c. */
                char path_disp[62];
                pal_strncpy(path_disp, path, sizeof(path_disp));
                ui_title(UI_PROMPT_OUT_ROW, UI_PROMPT_OUT_COL, green bold, path_disp);
            }
            else
                ui_status(STATUS_ERROR);
            pal_pause();
        }
        else if (pal_strcmp(cmd, "mdr") == 0)
        {
            if (pal_strlen(args) == 0) ui_status(STATUS_INVALID);
            else ui_status(pal_mdr(args) == 0 ? STATUS_SUCCESS : STATUS_ERROR);
        }
        else if (pal_strcmp(cmd, "rdr") == 0)
        {
            if (pal_strlen(args) == 0) ui_status(STATUS_INVALID);
            else ui_status(pal_rdr(args) == 0 ? STATUS_SUCCESS : STATUS_ERROR);
        }
        else if (pal_strcmp(cmd, "rmf") == 0)
        {
            if (pal_strlen(args) == 0) ui_status(STATUS_INVALID);
            else ui_status(pal_rmf(args) == 0 ? STATUS_SUCCESS : STATUS_ERROR);
        }
        else if (pal_strcmp(cmd, "rdf") == 0)
        {
            /* Same as ldr above: on success pal_rdf() draws its own full
               bordered frame and waits for a keypress. */
            if (pal_strlen(args) == 0) ui_status(STATUS_INVALID);
            else if (pal_rdf(args) != 0) ui_status(STATUS_ERROR);
        }
        else if (pal_strcmp(cmd, "cpf") == 0)
        {
            char a[128], b[128];
            split_two(args, a, sizeof(a), b, sizeof(b));
            if (pal_strlen(a) == 0 || pal_strlen(b) == 0) ui_status(STATUS_INVALID);
            else ui_status(pal_cpf(a, b) == 0 ? STATUS_SUCCESS : STATUS_ERROR);
        }
        else if (pal_strcmp(cmd, "mvf") == 0)
        {
            char a[128], b[128];
            split_two(args, a, sizeof(a), b, sizeof(b));
            if (pal_strlen(a) == 0 || pal_strlen(b) == 0) ui_status(STATUS_INVALID);
            else ui_status(pal_mvf(a, b) == 0 ? STATUS_SUCCESS : STATUS_ERROR);
        }
        else if (pal_strcmp(cmd, "cpdr") == 0)
        {
            char a[128], b[128];
            split_two(args, a, sizeof(a), b, sizeof(b));
            if (pal_strlen(a) == 0 || pal_strlen(b) == 0) ui_status(STATUS_INVALID);
            else ui_status(pal_cpdr(a, b) == 0 ? STATUS_SUCCESS : STATUS_ERROR);
        }
        else if (pal_strcmp(cmd, "mvdr") == 0)
        {
            char a[128], b[128];
            split_two(args, a, sizeof(a), b, sizeof(b));
            if (pal_strlen(a) == 0 || pal_strlen(b) == 0) ui_status(STATUS_INVALID);
            else ui_status(pal_mvdr(a, b) == 0 ? STATUS_SUCCESS : STATUS_ERROR);
        }
        else if (pal_strcmp(cmd, "rnmdr") == 0)
        {
            char a[128], b[128];
            split_two(args, a, sizeof(a), b, sizeof(b));
            if (pal_strlen(a) == 0 || pal_strlen(b) == 0) ui_status(STATUS_INVALID);
            else ui_status(pal_rnmdr(a, b) == 0 ? STATUS_SUCCESS : STATUS_ERROR);
        }
        else if (pal_strcmp(cmd, "notepad") == 0)
            oe_notepad_run(args); /* restores the bordered ui_init() frame before returning, per notepad.h */
        else
            ui_status(STATUS_INVALID);
    }
}

/* ================= MAIN MENU ================= */

static int show_main_menu(void)
{
    char input[16];

    for (;;)
    {
        draw_header();
        ui_title(ROW_MENU,     COL_CONTENT, white bold, "[1] Install OE Reboot");
        ui_title(ROW_MENU + 1, COL_CONTENT, white bold, "[2] Try OE Reboot (Guest Mode)");
        pal_set_cursor(ROW_MENU + 3, COL_CONTENT);
        pal_print("Choose an option (1/2): ");
        pal_readline(input, sizeof(input));

        if (pal_strcmp(input, "1") == 0) return 1;
        if (pal_strcmp(input, "2") == 0) return 2;
    }
}

void kernel_main(unsigned long magic, unsigned long mb_info_addr)
{
    unsigned int mb;
    char numbuf[16];

    /* This installer still never touches paging (stays identity-mapped as
       GRUB left it) or allocates via the PMM (heap.c's fixed static arena
       backs every kmalloc here, same as before) -- but it does call
       pmm_init() below purely so Try Mode's `sysinfo` (oe_systeminfo_entry(),
       via pal_get_oe_info()) can report real RAM numbers instead of the
       zero-initialized defaults it'd otherwise read. mb_info_addr is the
       same real multiboot info GRUB passes kernel.bin, since this binary is
       loaded exactly the same way (see this file's header comment). */
    (void)magic;

    hal_initialize();
    hal_debug_print("OE Reboot Installer starting.\n");
    pmm_init(mb_info_addr);

    /* Same loading screen the real OS shows on every boot -- app_id 0 means
       "just the logo/progress bar, no app title." */
    bootscreen_show(0);
    pal_sleep(1);
    pal_clear_screen();

    ui_init();
    ui_title(ROW_HEADER, COL_CONTENT, cyan bold, "OE Reboot -- Disk Installer");

    if (!hal_disk_init())
        fail("No disk detected.");

    g_sector_count = hal_disk_get_sector_count();
    mb = g_sector_count / 2048u; /* 512 B/sector * 2048 = 1 MB -- plain 32-bit
                                    math on purpose: installer.bin doesn't link
                                    libgcc's 64-bit divide helper for this. */

    {
        int n = 0;
        const char* model = hal_disk_get_model_string();
        while (model[n] && n < 40) { g_disk_line[n] = model[n]; n++; }
        g_disk_line[n++] = ' '; g_disk_line[n++] = '-'; g_disk_line[n++] = ' ';
        pal_itoa((int)mb, numbuf);
        {
            int k = 0;
            while (numbuf[k] && n < 60) g_disk_line[n++] = numbuf[k++];
        }
        g_disk_line[n++] = ' '; g_disk_line[n++] = 'M'; g_disk_line[n++] = 'B';
        g_disk_line[n] = '\0';
    }

    for (;;)
    {
        int choice = show_main_menu();
        if (choice == 1)
            do_install();  /* returns only if the user aborted the YES prompt */
        else
            try_mode();    /* returns when the user types `menu` */
    }
}
