#ifndef UL_VGA_H
#define UL_VGA_H
#include "types.h"
enum { VGA_BLACK, VGA_BLUE, VGA_GREEN, VGA_CYAN, VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LGREY,
       VGA_DGREY, VGA_LBLUE, VGA_LGREEN, VGA_LCYAN, VGA_LRED, VGA_LMAGENTA, VGA_YELLOW, VGA_WHITE };
void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_set_color(uint8_t fg, uint8_t bg);
#endif
