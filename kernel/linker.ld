ENTRY(_start)

SECTIONS
{
    . = 1M; /* GRUB loads kernels at 1MB by convention */

    kernel_start = .;

    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    .data : ALIGN(4K)
    {
        *(.data)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    kernel_end = .; /* pmm.c reserves [kernel_start, kernel_end) so the frame allocator never hands out the kernel's own image */
}
