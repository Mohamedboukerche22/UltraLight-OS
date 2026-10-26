#ifndef UL_SERIAL_H
#define UL_SERIAL_H
#include "types.h"
void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
bool serial_has_input(void);
int  serial_getc(void);   /* -1 if nothing pending */
#endif
