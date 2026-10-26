/* kernel/panic.c */
#include "panic.h"
#include "kprintf.h"
#include "console.h"
#include "io.h"

void panic(const char *fmt, ...) {
    cpu_cli();
    console_color(VGA_LRED);
    kprintf("\nKERNEL PANIC: ");
    va_list ap; va_start(ap, fmt);
    kvprintf_cb(console_putc, fmt, ap);
    va_end(ap);
    kprintf("\nSystem halted.\n");
    for (;;) cpu_hlt();
}
