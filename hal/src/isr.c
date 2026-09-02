/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: isr.c
About: The 32 CPU exception handlers (vectors 0-31), via GCC's
       __attribute__((interrupt)) rather than hand-rolled assembly
       stubs. No process model exists yet (single ring-0 kernel, no
       scheduler), so every fault is unrecoverable by definition --
       isr_fault() prints the exception name/vector/error code (plus
       CR2 on a page fault) to both VGA and serial, then halts.
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT)
------------------------------------------------------------
*/

#include "isr.h"
#include "idt.h"
#include "vga.h"
#include "serial.h"

struct interrupt_frame;

static const char* const exception_names[32] = {
    "Division By Zero", "Debug", "Non-Maskable Interrupt", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 Floating-Point Exception", "Alignment Check", "Machine Check", "SIMD Floating-Point Exception",
    "Virtualization Exception", "Control Protection Exception", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection Exception", "VMM Communication Exception", "Security Exception", "Reserved"
};

static void print_hex(unsigned int val)
{
    static const char digits[] = "0123456789ABCDEF";
    char buf[11];
    int i;

    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; i++)
        buf[2 + i] = digits[(val >> ((7 - i) * 4)) & 0xF];
    buf[10] = '\0';

    vga_print(buf);
    serial_print(buf);
}

static unsigned int read_cr2(void)
{
    unsigned int val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
}

/*
   No process model exists yet (single ring-0 kernel, no scheduler) -- there
   is nothing to terminate and resume other than the whole system, so every
   fault is currently unrecoverable by definition. This becomes "kill the
   offending task" once processes exist; until then, halting with full
   diagnostics beats silently corrupting state or triple-faulting blind.
*/
static void isr_fault(int n, unsigned int err)
{
    vga_print("\n\033[41;97m*** EXCEPTION: ");
    vga_print(exception_names[n]);
    vga_print(" ***\033[0m\n");
    serial_print("\n*** EXCEPTION: ");
    serial_print(exception_names[n]);
    serial_print(" ***\n");

    vga_print("vector: ");
    serial_print("vector: ");
    print_hex((unsigned int)n);
    vga_print("  error code: ");
    serial_print("  error code: ");
    print_hex(err);

    if (n == 14) /* page fault: CR2 holds the faulting linear address */
    {
        vga_print("\nfaulting address: ");
        serial_print("\nfaulting address: ");
        print_hex(read_cr2());
    }

    vga_print("\nsystem halted.\n");
    serial_print("\nsystem halted.\n");

    __asm__ volatile ("cli");
    for (;;)
        __asm__ volatile ("hlt");
}

#define DEFINE_ISR_NOERR(n) \
static __attribute__((interrupt)) void isr##n(struct interrupt_frame *frame) \
{ (void)frame; isr_fault(n, 0); }

#define DEFINE_ISR_ERR(n) \
static __attribute__((interrupt)) void isr##n(struct interrupt_frame *frame, unsigned int err) \
{ (void)frame; isr_fault(n, err); }

DEFINE_ISR_NOERR(0)
DEFINE_ISR_NOERR(1)
DEFINE_ISR_NOERR(2)
DEFINE_ISR_NOERR(3)
DEFINE_ISR_NOERR(4)
DEFINE_ISR_NOERR(5)
DEFINE_ISR_NOERR(6)
DEFINE_ISR_NOERR(7)
DEFINE_ISR_ERR(8)
DEFINE_ISR_NOERR(9)
DEFINE_ISR_ERR(10)
DEFINE_ISR_ERR(11)
DEFINE_ISR_ERR(12)
DEFINE_ISR_ERR(13)
DEFINE_ISR_ERR(14)
DEFINE_ISR_NOERR(15)
DEFINE_ISR_NOERR(16)
DEFINE_ISR_ERR(17)
DEFINE_ISR_NOERR(18)
DEFINE_ISR_NOERR(19)
DEFINE_ISR_NOERR(20)
DEFINE_ISR_ERR(21)
DEFINE_ISR_NOERR(22)
DEFINE_ISR_NOERR(23)
DEFINE_ISR_NOERR(24)
DEFINE_ISR_NOERR(25)
DEFINE_ISR_NOERR(26)
DEFINE_ISR_NOERR(27)
DEFINE_ISR_NOERR(28)
DEFINE_ISR_ERR(29)
DEFINE_ISR_ERR(30)
DEFINE_ISR_NOERR(31)

void isr_install(void)
{
    idt_set_gate(0,  (unsigned int)isr0,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(1,  (unsigned int)isr1,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(2,  (unsigned int)isr2,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(3,  (unsigned int)isr3,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(4,  (unsigned int)isr4,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(5,  (unsigned int)isr5,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(6,  (unsigned int)isr6,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(7,  (unsigned int)isr7,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(8,  (unsigned int)isr8,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(9,  (unsigned int)isr9,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(10, (unsigned int)isr10, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(11, (unsigned int)isr11, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(12, (unsigned int)isr12, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(13, (unsigned int)isr13, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(14, (unsigned int)isr14, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(15, (unsigned int)isr15, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(16, (unsigned int)isr16, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(17, (unsigned int)isr17, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(18, (unsigned int)isr18, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(19, (unsigned int)isr19, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(20, (unsigned int)isr20, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(21, (unsigned int)isr21, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(22, (unsigned int)isr22, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(23, (unsigned int)isr23, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(24, (unsigned int)isr24, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(25, (unsigned int)isr25, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(26, (unsigned int)isr26, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(27, (unsigned int)isr27, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(28, (unsigned int)isr28, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(29, (unsigned int)isr29, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(30, (unsigned int)isr30, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(31, (unsigned int)isr31, IDT_KERNEL_CS, IDT_GATE_INT32);
}
