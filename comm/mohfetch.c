/* comm/mohfetch.c - neofetch-style system summary
 *
 * Prints an ASCII logo on the left and live kernel/hardware information on
 * the right.  Colors are VGA-only; serial output stays plain text so it is
 * still readable on a headless machine.
 */
#include "comm.h"
#include "console.h"
#include "kprintf.h"
#include "string.h"
#include "io.h"
#include "pit.h"
#include "pmm.h"
#include "multiboot.h"
#include "version.h"

#define LOGO_ROWS 9

static const char *logo[LOGO_ROWS] = {
    "      /\\      ",
    "     /  \\     ",
    "    / /\\ \\    ",
    "   / /  \\ \\   ",
    "  / /    \\ \\  ",
    " / /______\\ \\ ",
    " \\__________/ ",
    "   ULTRALIGHT ",
    "              ",
};

static void cpu_brand(char *out, size_t cap) {
    uint32_t a, b, c, d;
    uint32_t max_ext;
    char raw[49];
    raw[48] = 0;

    cpuid(0x80000000u, &max_ext, &b, &c, &d);
    if (max_ext < 0x80000004u) {
        strncpy(out, "unknown", cap - 1);
        out[cap - 1] = 0;
        return;
    }
    for (uint32_t i = 0; i < 3; i++) {
        cpuid(0x80000002u + i, &a, &b, &c, &d);
        memcpy(raw + i * 16,      &a, 4);
        memcpy(raw + i * 16 + 4,  &b, 4);
        memcpy(raw + i * 16 + 8,  &c, 4);
        memcpy(raw + i * 16 + 12, &d, 4);
    }

    const char *p = raw;
    while (*p == ' ') p++;
    size_t n = strlen(p);
    while (n > 0 && p[n - 1] == ' ') n--;
    if (n >= cap) n = cap - 1;
    memcpy(out, p, n);
    out[n] = 0;
}

static void info_row(const char *art, const char *label, const char *fmt, ...) {
    console_color(VGA_LCYAN);
    kprintf("%s", art);
    console_color(VGA_LGREY);
    kprintf("  ");
    if (label) {
        console_color(VGA_LGREEN);
        kprintf("%s", label);
        console_color(VGA_LGREY);
    }
    va_list ap;
    va_start(ap, fmt);
    kvprintf_cb(console_putc, fmt, ap);
    va_end(ap);
    kprintf("\n");
}

static void cmd_mohfetch(int argc, char **argv) {
    (void)argc; (void)argv;

    char cpu[49];
    cpu_brand(cpu, sizeof cpu);

    uint64_t ticks = pit_ticks(), secs = ticks / PIT_HZ;
    uint64_t total_kib = pmm_total_frames() * (PAGE_SIZE / 1024);
    uint64_t free_kib = pmm_free_frames() * (PAGE_SIZE / 1024);
    uint64_t used_kib = total_kib - free_kib;

    const bootinfo_t *bi = multiboot_bootinfo();

    info_row(logo[0], NULL,       "ultralight@x86_64");
    info_row(logo[1], "OS:     ", "UltraLight OS v%s", UL_VERSION);
    info_row(logo[2], "Kernel: ", "%s %s (x86_64, freestanding)", UL_CODENAME, UL_VERSION);
    info_row(logo[3], "Built:  ", "%s %s", __DATE__, __TIME__);
    info_row(logo[4], "Uptime: ", "%llu:%02llu:%02llu",
             (unsigned long long)(secs / 3600),
             (unsigned long long)(secs / 60 % 60),
             (unsigned long long)(secs % 60));
    info_row(logo[5], "CPU:    ", "%s", cpu);
    info_row(logo[6], "Memory: ", "%llu.%02llu MiB used / %llu MiB total",
             (unsigned long long)(used_kib / 1024),
             (unsigned long long)((used_kib % 1024) * 100 / 1024),
             (unsigned long long)(total_kib / 1024));
    if (bi)
        info_row(logo[7], "Boot:   ", "%s (multiboot%d)",
                 bi->loader[0] ? bi->loader : "unknown", bi->protocol);
    else
        info_row(logo[7], "Boot:   ", "unknown");
    info_row(logo[8], "Shell:  ", "ultralight-sh, %llu commands",
             (unsigned long long)comm_count());

    console_color(VGA_LGREY);
}

static void cmd_fetch(int argc, char **argv) {
    cmd_mohfetch(argc, argv);
}

COMMAND("mohfetch", "neofetch-style system summary", cmd_mohfetch);
COMMAND("fetch", "alias for mohfetch", cmd_fetch);
