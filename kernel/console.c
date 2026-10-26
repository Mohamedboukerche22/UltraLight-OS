/* kernel/console.c - unified console: output to VGA + serial, input from keyboard + serial */
#include "console.h"
#include "serial.h"
#include "keyboard.h"
#include "io.h"

void console_putc(char c) {
    vga_putc(c);
    if (c == '\b') { serial_putc('\b'); return; }
    serial_putc(c);
}

void console_puts(const char *s) { while (*s) console_putc(*s++); }

void console_color(uint8_t fg) { vga_set_color(fg, VGA_BLACK); }

void console_clear(void) {
    vga_clear();
    serial_puts("\033[2J\033[H");
}

int console_getc(void) {
    for (;;) {
        cpu_cli();
        int k = keyboard_poll();
        if (k < 0) {
            int s = serial_getc();                  /* serial terminal (qemu -nographic) */
            if (s >= 0) {
                k = s;
                if (k == '\r') k = '\n';
                else if (k == 0x7F) k = '\b';
            }
        }
        if (k >= 0) { cpu_sti(); return k; }
        cpu_idle();                                 /* sleep until next interrupt (timer ticks at 100 Hz) */
    }
}
