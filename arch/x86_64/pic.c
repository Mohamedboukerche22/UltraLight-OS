/* arch/x86_64/pic.c - 8259 PIC: remap IRQs to vectors 32-47 */
#include "pic.h"
#include "io.h"

#define PIC1_CMD 0x20
#define PIC1_DAT 0x21
#define PIC2_CMD 0xA0
#define PIC2_DAT 0xA1

void pic_init(void) {
    outb(PIC1_CMD, 0x11); io_wait();          /* ICW1: init + ICW4 */
    outb(PIC2_CMD, 0x11); io_wait();
    outb(PIC1_DAT, 0x20); io_wait();          /* ICW2: vector offsets */
    outb(PIC2_DAT, 0x28); io_wait();
    outb(PIC1_DAT, 0x04); io_wait();          /* ICW3: slave on IRQ2 */
    outb(PIC2_DAT, 0x02); io_wait();
    outb(PIC1_DAT, 0x01); io_wait();          /* ICW4: 8086 mode */
    outb(PIC2_DAT, 0x01); io_wait();
    outb(PIC1_DAT, 0xFB);                     /* mask everything except the cascade (IRQ2) */
    outb(PIC2_DAT, 0xFF);
}

void pic_unmask(uint8_t irq) {
    if (irq < 8) outb(PIC1_DAT, inb(PIC1_DAT) & ~(1u << irq));
    else         outb(PIC2_DAT, inb(PIC2_DAT) & ~(1u << (irq - 8)));
}

void pic_mask(uint8_t irq) {
    if (irq < 8) outb(PIC1_DAT, inb(PIC1_DAT) | (1u << irq));
    else         outb(PIC2_DAT, inb(PIC2_DAT) | (1u << (irq - 8)));
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

/* IRQ7 / IRQ15 can fire spuriously; the ISR register tells us if it was real. */
bool pic_is_spurious(uint8_t irq) {
    if (irq == 7) {
        outb(PIC1_CMD, 0x0B);
        return !(inb(PIC1_CMD) & 0x80);
    }
    if (irq == 15) {
        outb(PIC2_CMD, 0x0B);
        if (!(inb(PIC2_CMD) & 0x80)) { outb(PIC1_CMD, 0x20); return true; }
    }
    return false;
}
