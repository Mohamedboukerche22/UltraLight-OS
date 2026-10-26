#ifndef UL_CONSOLE_H
#define UL_CONSOLE_H
#include "types.h"
#include "vga.h"
void console_putc(char c);
void console_puts(const char *s);
void console_color(uint8_t fg);
void console_clear(void);
int  console_getc(void);   /* blocks; keyboard (incl. KEY_* codes) or serial */
#endif
