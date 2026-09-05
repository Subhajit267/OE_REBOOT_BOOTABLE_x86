
# Project Notes

## History
The core shell was originally designed way back in school using C and C++.
That codebase was quite messy (links at the end if you want to visit it).

In December 2025 I decided to restart the project with a newer
architecture, starting by isolating platform-specific code for Linux,
Windows, and a future kernel port into the PAL layer. That isolation work
, along with pre kernel development is entirely my own.

## Kernel Subsystem
An earlier in-progress kernel/HAL/bootloader attempt was lost to an SSD
failure, with no backup in place at the time, so it was rebuilt from
scratch — which is why its file dates run newer than the rest of the
codebase.

For this part I used AI for boilerplate and guidance, since I was new to
OS/kernel development — the same way I leaned on Stack Overflow back then.
Not every line here is mine, but real bugs like the ANSI-VGA color
mismatch, the Notepad stray-character keyboard IRQ bug, and the ACPI
shutdown fault were debugged and fixed by me directly, verified live in
QEMU, mtools, VMware, and VirtualBox. Whatever this counts as, I learned a
lot about how an OS actually works, how drivers are written, and what
things look like underneath the shell — call it vibecoding if you like.

## Links
- https://sites.google.com/view/operatingenvironment/ — a site I made back
  in 2020 and updated until 2022, hosting some of the oldest versions.
- https://github.com/Subhajit267/Operating-Environment-for-windows-C-and-
  Cpp-based — the original windows specific version of operating environment written in C and C++
- https://github.com/Subhajit267/Original-Operating-Environment-2020-Aug-Jul-cpp-variant-for-linux-
  — the original linux specific versions which ran on onlinegdb
- https://github.com/Subhajit267/OE-REBOOT — the reboot version with new architecture and PALayers
