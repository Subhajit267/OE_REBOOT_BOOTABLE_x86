# Bare-metal kernel build (separate from the hosted OE REBOOT V1 VS project).
# Build/run inside WSL — needs: gcc, gcc-multilib, nasm, grub-pc-bin, xorriso, qemu-system-x86, mtools.

CC  = gcc
AS  = nasm
LD  = ld

# Every module login()'s full call chain (login -> prompt -> the system_tools
# shell, per main.c) transitively reaches. hal/kernel/pal are this kernel's
# own; everything else is the same hosted-app source the VS project builds,
# unmodified except where a file needed a freestanding-specific fix (see
# individual file comments for those).
APP_INCLUDES = -Iapp_installer/include -Iextras_and_info/include -Ifile/include \
               -Ihelp_docs/include -Isetup/include -Isystem_core/include \
               -Isystem_tools/calculator/include -Isystem_tools/notepad/include \
               -Isystem_tools/regedit/include -Isystem_tools/settings/include \
               -Isystem_tools/systeminfo/include -Iui/include -Iui_strings/include \
               -Iuser_management/include -Iutilities/include

CFLAGS  = -m32 -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector \
          -fno-pie -Wall -Wextra -Ikernel/include -Ihal/include -Ipal/include \
          -Iinstaller/include $(APP_INCLUDES) -O2 -mgeneral-regs-only
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T kernel/linker.ld -nostdlib

# libgcc.a -- pure compiler-support routines, no OS dependencies, safe
# under -nostdlib. This target's libgcc has __udivdi3/__udivmoddi4 (64-bit
# integer divide -- systeminfo.c needs these regardless of the FPU flag
# below) but, being built for a host where hardware FP is a given, no
# soft-float double routines (__adddf3 etc.) at all -- confirmed via `nm`.
# So integer division is covered by linking this; float still needs real
# FPU instructions, handled by FPU_OBJS below.
LIBGCC = $(shell $(CC) -m32 -print-libgcc-file-name)

OBJS = boot/src/boot.o \
       kernel/src/kernel.o \
       kernel/src/pmm.o \
       kernel/src/paging.o \
       kernel/src/fat.o \
       kernel/src/mbr.o \
       kernel/src/heap.o \
       hal/src/hal.o \
       hal/src/acpi.o \
       hal/src/vga.o \
       hal/src/serial.o \
       hal/src/gdt.o \
       hal/src/idt.o \
       hal/src/isr.o \
       hal/src/irq.o \
       hal/src/pic.o \
       hal/src/pit.o \
       hal/src/keyboard.o \
       hal/src/ata.o \
       pal/src/pal_kernel.o \
       app_installer/src/app_installer.o \
       app_installer/src/app_table.o \
       extras_and_info/src/improvements.o \
       extras_and_info/src/source_display.o \
       file/src/file.o \
       help_docs/src/prompt_help.o \
       help_docs/src/regedit_help.o \
       help_docs/src/settings_help.o \
       setup/src/installer.o \
       system_core/src/prompt.o \
       system_tools/calculator/src/calculator.o \
       system_tools/notepad/src/notepad.o \
       system_tools/notepad/src/notepad_edit.o \
       system_tools/notepad/src/notepad_main.o \
       system_tools/notepad/src/notepad_view.o \
       system_tools/regedit/src/regedit.o \
       system_tools/settings/src/settings.o \
       system_tools/systeminfo/src/systeminfo.o \
       ui/src/bootscreen.o \
       ui/src/ui_elements.o \
       ui_strings/src/ui_setup.o \
       user_management/src/login.o \
       user_management/src/password_management.o \
       user_management/src/user_creation.o \
       user_management/src/user_guest.o \
       user_management/src/user_id_change.o \
       utilities/src/activation.o \
       utilities/src/input_validation.o \
       utilities/src/password_hash.o \
       utilities/src/timer.o

DISK_IMG_SECTORS = 32768 # 16MB -- comfortably in FAT16 territory (see fat.c's cluster-count comment)

.PHONY: all run run-bootdisk run-installer vmware virtualbox clean

all: os.iso disk.img

boot/src/boot.o: boot/src/boot.s
	$(AS) $(ASFLAGS) $< -o $@

# One generic rule handles every .c anywhere in the tree -- GNU Make's %
# stem matches directory components too, so this covers kernel/, hal/,
# pal/, and every app module without a per-directory rule each.
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Files that do real `double` arithmetic -- -mgeneral-regs-only forbids the
# FPU entirely (needed elsewhere to keep interrupt-attribute ISRs legal, see
# isr.c's comment), and this libgcc has no soft-float routines to fall back
# on (see LIBGCC's comment above), so these need actual x87 instructions.
# None of them touch a saved interrupt frame, so that's safe. Discovered via
# the linker's undefined-symbol list, not guessed up front -- if a new file
# starts failing to link the same way, add its .o here.
FPU_OBJS = pal/src/pal_kernel.o \
           system_tools/calculator/src/calculator.o \
           ui/src/ui_elements.o \
           utilities/src/input_validation.o

$(FPU_OBJS): CFLAGS := $(filter-out -mgeneral-regs-only,$(CFLAGS))

# acpi.c deliberately dereferences fixed low-physical-memory addresses
# (BIOS data area / ROM area, e.g. the EBDA segment word at 0x40E) -- real,
# correct freestanding/OS-dev code, but GCC's -Warray-bounds heuristic
# misreads a literal-address cast-and-dereference as indexing a zero-sized
# array at address 0 and false-positives on it. Disabling just this one
# warning for just this one file is the standard fix for this exact,
# well-known false positive.
hal/src/acpi.o: CFLAGS += -Wno-array-bounds

kernel.bin: $(OBJS) kernel/linker.ld
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJS) $(LIBGCC)

os.iso: kernel.bin boot/grub.cfg
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/kernel.bin
	cp boot/grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o os.iso isodir 2>/dev/null

disk.img: diskdir/HELLO.TXT
	dd if=/dev/zero of=disk.img bs=512 count=$(DISK_IMG_SECTORS) 2>/dev/null
	mformat -i disk.img ::
	mcopy -i disk.img diskdir/HELLO.TXT ::HELLO.TXT

run: os.iso disk.img
	qemu-system-i386 -boot d -drive file=disk.img,format=raw,if=ide -cdrom os.iso

# ---- Stage 4: standalone bootloader (no GRUB/ISO involved at all) ----
# Fixed disk layout: LBA0 = MBR (bootloader/src/mbr.asm), LBA1.. = Stage 2
# (bootloader/src/stage2.asm), LBA17.. = kernel.bin's flat (non-ELF) image,
# then a real MBR-declared FAT partition starting at the conventional
# 1MB-aligned LBA 2048 -- comfortably past the largest this project's
# kernel.bin has ever been (currently ~115KB / 225 sectors), formatted at
# boot by fat_format_at() (kernel/src/mbr.c + kernel/src/fat.c) exactly
# like a genuinely blank disk would be. This target is dev/test scratch
# for the bootloader itself -- Stage 5's real installer writes this same
# layout onto a target disk from a running installer, not from `make`.
BOOTDISK_SECTORS    = 131072  # 64MB
BOOTDISK_PART_START = 2048
BOOTDISK_KERNEL_LBA = 17

# The three raw binaries the boot chain needs on a target disk: the MBR
# bootstrap (mbr.bin, exactly 512 bytes), Stage 2 (stage2.bin), and the
# kernel's flat (non-ELF) image (kernel.flat.bin). Split out of what used to
# be bootdisk.img's own recipe so Stage 5's installer.bin can embed these
# same three files (see installer/tools/gen_installer_assets.py) instead of
# only `make bootdisk.img` being able to produce them.
mbr.bin stage2.bin kernel.flat.bin: kernel.bin bootloader/src/mbr.asm bootloader/src/stage2.asm
	objcopy -O binary kernel.bin kernel.flat.bin
	ENTRY=$$(readelf -h kernel.bin | grep "Entry point" | awk '{print $$NF}'); \
	KSECTORS=$$(( ($$(stat -c%s kernel.flat.bin) + 511) / 512 )); \
	nasm -f bin bootloader/src/stage2.asm -o stage2.bin \
	    -D KERNEL_LBA=$(BOOTDISK_KERNEL_LBA) -D KERNEL_SECTORS=$$KSECTORS -D KERNEL_ENTRY=$$ENTRY; \
	STAGE2_SECTORS=$$(( ($$(stat -c%s stage2.bin) + 511) / 512 )); \
	nasm -f bin bootloader/src/mbr.asm -o mbr.bin \
	    -D STAGE2_LBA=1 -D STAGE2_SECTORS=$$STAGE2_SECTORS

bootdisk.img: mbr.bin stage2.bin kernel.flat.bin bootloader/tools/write_mbr_partition.py
	dd if=/dev/zero of=bootdisk.img bs=512 count=$(BOOTDISK_SECTORS) 2>/dev/null
	dd if=mbr.bin of=bootdisk.img bs=512 seek=0 conv=notrunc 2>/dev/null
	dd if=stage2.bin of=bootdisk.img bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=kernel.flat.bin of=bootdisk.img bs=512 seek=$(BOOTDISK_KERNEL_LBA) conv=notrunc 2>/dev/null
	python3 bootloader/tools/write_mbr_partition.py bootdisk.img $(BOOTDISK_PART_START) \
	    $$(( $(BOOTDISK_SECTORS) - $(BOOTDISK_PART_START) ))

# Boots bootdisk.img alone -- no -cdrom, no GRUB, no ISO anywhere on the
# command line. Reaching the bootscreen/installer here is the actual proof
# this stage works, not just that it assembles.
run-bootdisk: bootdisk.img
	qemu-system-i386 -drive file=bootdisk.img,format=raw,if=ide -boot c -m 128

# ---- Stage 5: standalone disk installer ----
# A separate, minimal boot target (its own kernel_main-equivalent, entirely
# isolated from the real OS's kernel.bin) that writes the same mbr.bin/
# stage2.bin/kernel.flat.bin boot chain (built above) plus a fresh OEFS/
# FAT32 partition onto whatever disk it finds, using hal_disk_write_sector/
# mbr_write()/fat_format_at() directly. Reuses boot/src/boot.s and
# kernel/linker.ld unchanged -- GRUB loads installer.bin exactly like
# kernel.bin, calling the same `kernel_main` symbol, which here is
# installer_main.c's installer flow instead of the real OS.
# pal_kernel.o + heap.o + pmm.o + the ui_elements/bootscreen/ui_setup/file
# chain are linked in so the installer can use the exact same bordered,
# colored screens (layout()/ui_title()/bootscreen_show()) as the real OS --
# these only depend on pal.h/file.h (confirmed by inspection: no
# user_management/system_tools symbols anywhere in them), so this is not the
# dependency wall it might look like.
# 2026-09-01: installer_main.c gained a live-CD style Install/Try menu --
# Try Mode non-destructively attempts fat_mount()/fat_mount_at() (never
# formats) and, if that succeeds, calls pal_init() and runs a small file/dir
# + notepad + sysinfo shell, redrawn via ui_init() at the real prompt()'s own
# UI_PROMPT_*/UI_STATUS_ROW coordinates for interface consistency. The
# notepad/*.o and systeminfo.o files below back that -- confirmed by
# inspection (same as the rest of this list) to depend on nothing outside
# pal.h/ui_setup.h/branding.h, except systeminfo.c's use of user.h's
# current_user/user_exists() -- those two are defined directly in
# installer_main.c (always "no user, GUEST") instead of linking the rest of
# user_management just for them. fat.c's own `mounted` guard still makes
# every FAT call fail closed on a genuinely blank disk instead of crashing,
# same as before this menu existed. pmm_init() IS now called for real (with
# the real multiboot info GRUB passes this binary, same as kernel.bin) so
# `sysinfo` can report real RAM numbers via pal_get_oe_info() -- paging is
# still never touched, and heap.c's kmalloc still isn't PMM-backed, so this
# has no effect beyond that reporting.
# 2026-09-01: paging.o added purely to satisfy hal/src/acpi.o's
# paging_get_identity_limit() symbol reference (a real page-fault this
# function fixed -- see acpi.c's own comment) -- paging_init() is still never
# called here, so that function always returns 0 in this binary, meaning
# acpi_power_off() always fails closed (ACPI shutdown unavailable, falls
# back to a bare hlt loop) rather than ever dereferencing a physical address
# with no MMU restriction actually in force. Acceptable: the installer's own
# `fail()`/pal_exit() path only needs a safe halt, not a graceful ACPI S5.
INSTALLER_OBJS = boot/src/boot.o \
                 installer/src/installer_main.o \
                 kernel/src/fat.o \
                 kernel/src/mbr.o \
                 kernel/src/pmm.o \
                 kernel/src/paging.o \
                 kernel/src/heap.o \
                 pal/src/pal_kernel.o \
                 ui/src/ui_elements.o \
                 ui/src/bootscreen.o \
                 ui_strings/src/ui_setup.o \
                 file/src/file.o \
                 system_tools/notepad/src/notepad.o \
                 system_tools/notepad/src/notepad_edit.o \
                 system_tools/notepad/src/notepad_main.o \
                 system_tools/notepad/src/notepad_view.o \
                 system_tools/systeminfo/src/systeminfo.o \
                 hal/src/hal.o \
                 hal/src/acpi.o \
                 hal/src/vga.o \
                 hal/src/serial.o \
                 hal/src/gdt.o \
                 hal/src/idt.o \
                 hal/src/isr.o \
                 hal/src/irq.o \
                 hal/src/pic.o \
                 hal/src/pit.o \
                 hal/src/keyboard.o \
                 hal/src/ata.o

installer/include/installer_assets.h: mbr.bin stage2.bin kernel.flat.bin installer/tools/gen_installer_assets.py
	mkdir -p installer/include
	python3 installer/tools/gen_installer_assets.py mbr.bin stage2.bin kernel.flat.bin installer/include/installer_assets.h

installer/src/installer_main.o: installer/include/installer_assets.h

installer.bin: $(INSTALLER_OBJS) kernel/linker.ld
	$(LD) $(LDFLAGS) -o installer.bin $(INSTALLER_OBJS) $(LIBGCC)

installer.iso: installer.bin boot/grub.cfg
	mkdir -p installer_isodir/boot/grub
	cp installer.bin installer_isodir/boot/kernel.bin
	cp boot/grub.cfg installer_isodir/boot/grub/grub.cfg
	grub-mkrescue -o installer.iso installer_isodir 2>/dev/null

# Boots the installer as -cdrom (the USB/CD install media stand-in) against
# whatever disk.img currently is (the "target HDD" stand-in) -- run
# `dd if=/dev/zero of=disk.img bs=1M count=64` first to simulate a genuinely
# blank drive, matching the real-hardware install flow this project targets.
run-installer: installer.iso
	qemu-system-i386 -boot d -drive file=disk.img,format=raw,if=ide -cdrom installer.iso

# Regenerates disk.vmdk from disk.img -- OE_Reboot.vmx (checked in, not
# rebuilt here) points at both this and os.iso as ide0:0/ide1:0. VMware's
# disk must land on IDE, not its default SCSI/NVMe for a new VM, or
# hal/src/ata.c's primary-bus probe (ports 0x1F0) finds nothing.
vmware: os.iso disk.img
	qemu-img convert -f raw -O vmdk disk.img disk.vmdk

# Regenerates disk.vdi (VirtualBox's native format) from disk.img. Same
# IDE-not-SATA/SCSI requirement as vmware's disk.vmdk -- see that target's
# comment.
virtualbox: os.iso disk.img
	qemu-img convert -f raw -O vdi disk.img disk.vdi

clean:
	rm -f $(OBJS) kernel.bin os.iso disk.img disk.vmdk disk.vdi
	rm -f kernel.flat.bin mbr.bin stage2.bin bootdisk.img
	rm -f installer/src/installer_main.o installer/include/installer_assets.h
	rm -f installer.bin installer.iso
	rm -rf isodir installer_isodir
