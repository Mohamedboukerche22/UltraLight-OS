/* drivers/vga.c - 80x25 text mode console */
#include "vga.h"
#include "io.h"
#include "string.h"

#define VGA_W 80
#define VGA_H 25
static volatile uint16_t *const vga_mem = (volatile uint16_t *)0xB8000;
static int cur_row, cur_col;
static uint8_t attr = 0x07;

static inline uint16_t cell(char c) { return (uint16_t)((uint16_t)attr << 8) | (uint8_t)c; }

static void update_cursor(void) {
    uint16_t pos = (uint16_t)(cur_row * VGA_W + cur_col);
    outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E); outb(0x3D5, (pos >> 8) & 0xFF);
}

void vga_set_color(uint8_t fg, uint8_t bg) { attr = (uint8_t)((bg << 4) | (fg & 0x0F)); }

void vga_clear(void) {
    for (int i = 0; i < VGA_W * VGA_H; i++) vga_mem[i] = cell(' ');
    cur_row = cur_col = 0;
    update_cursor();
}

void vga_init(void) {
    outb(0x3D4, 0x0A); outb(0x3D5, (inb(0x3D5) & 0xC0) | 14);   /* enable underline cursor */
    outb(0x3D4, 0x0B); outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
    vga_set_color(VGA_LGREY, VGA_BLACK);
    vga_clear();
}

static void scroll(void) {
    for (int i = 0; i < VGA_W * (VGA_H - 1); i++) vga_mem[i] = vga_mem[i + VGA_W];
    for (int i = VGA_W * (VGA_H - 1); i < VGA_W * VGA_H; i++) vga_mem[i] = cell(' ');
    cur_row = VGA_H - 1;
}

void vga_putc(char c) {
    switch (c) {
    case '\n': cur_col = 0; cur_row++; break;
    case '\r': cur_col = 0; break;
    case '\b': if (cur_col > 0) cur_col--; break;
    case '\t': cur_col = (cur_col + 8) & ~7; break;
    default:
        if ((unsigned char)c < 32) break;
        vga_mem[cur_row * VGA_W + cur_col] = cell(c);
        cur_col++;
    }
    if (cur_col >= VGA_W) { cur_col = 0; cur_row++; }
    if (cur_row >= VGA_H) scroll();
    update_cursor();
}
