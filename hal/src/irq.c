/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-08-30
Module: HAL
File: irq.c
About: The 16 IRQ stubs (vectors 32-47), each dispatching to whatever
       handler irq_install_handler() registered for that line (pit.c's
       tick counter, keyboard.c's scancode handler) and sending the PIC
       its end-of-interrupt.
Revisions:
- 2026-08-30  Initial creation (Phase 2: interrupts/IDT/IRQ/PIC/PIT)
------------------------------------------------------------
*/

#include "irq.h"
#include "idt.h"
#include "pic.h"

struct interrupt_frame;

static irq_handler_t irq_handlers[16];

void irq_install_handler(int irq, irq_handler_t handler)
{
    irq_handlers[irq] = handler;
}

static void irq_common(int irq)
{
    if (irq_handlers[irq])
        irq_handlers[irq]();

    pic_send_eoi(irq);
}

#define DEFINE_IRQ(n) \
static __attribute__((interrupt)) void irq_stub##n(struct interrupt_frame *frame) \
{ (void)frame; irq_common(n); }

DEFINE_IRQ(0)
DEFINE_IRQ(1)
DEFINE_IRQ(2)
DEFINE_IRQ(3)
DEFINE_IRQ(4)
DEFINE_IRQ(5)
DEFINE_IRQ(6)
DEFINE_IRQ(7)
DEFINE_IRQ(8)
DEFINE_IRQ(9)
DEFINE_IRQ(10)
DEFINE_IRQ(11)
DEFINE_IRQ(12)
DEFINE_IRQ(13)
DEFINE_IRQ(14)
DEFINE_IRQ(15)

void irq_install(void)
{
    idt_set_gate(IRQ_BASE + 0,  (unsigned int)irq_stub0,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 1,  (unsigned int)irq_stub1,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 2,  (unsigned int)irq_stub2,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 3,  (unsigned int)irq_stub3,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 4,  (unsigned int)irq_stub4,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 5,  (unsigned int)irq_stub5,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 6,  (unsigned int)irq_stub6,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 7,  (unsigned int)irq_stub7,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 8,  (unsigned int)irq_stub8,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 9,  (unsigned int)irq_stub9,  IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 10, (unsigned int)irq_stub10, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 11, (unsigned int)irq_stub11, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 12, (unsigned int)irq_stub12, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 13, (unsigned int)irq_stub13, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 14, (unsigned int)irq_stub14, IDT_KERNEL_CS, IDT_GATE_INT32);
    idt_set_gate(IRQ_BASE + 15, (unsigned int)irq_stub15, IDT_KERNEL_CS, IDT_GATE_INT32);
}
