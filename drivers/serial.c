/* drivers/serial.c - 16550 UART on COM1 (115200 8N1) */
#include "serial.h"
#include "io.h"

#define COM1 0x3F8
static bool present;

void serial_init(void) {
    outb(COM1 + 1, 0x00);        /* no interrupts (we poll) */
    outb(COM1 + 3, 0x80);        /* DLAB on */
    outb(COM1 + 0, 0x01);        /* divisor 1 -> 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);        /* 8N1, DLAB off */
    outb(COM1 + 2, 0xC7);        /* enable + clear FIFOs */
    outb(COM1 + 4, 0x1E);        /* loopback self-test */
    outb(COM1 + 0, 0xAE);
    if (inb(COM1) != 0xAE) { present = false; return; }
    outb(COM1 + 4, 0x0F);        /* normal operation, OUT1/OUT2 set */
    present = true;
}

void serial_putc(char c) {
    if (!present) return;
    if (c == '\n') serial_putc('\r');
    for (int i = 0; i < 100000 && !(inb(COM1 + 5) & 0x20); i++) { }
    outb(COM1, (uint8_t)c);
}

void serial_puts(const char *s) { while (*s) serial_putc(*s++); }

bool serial_has_input(void) { return present && (inb(COM1 + 5) & 0x01); }

int serial_getc(void) { return serial_has_input() ? inb(COM1) : -1; }
