/* kernel/shell.c - built-in kernel monitor shell (temporary until userland exists) */
#include "shell.h"
#include "console.h"
#include "kprintf.h"
#include "string.h"
#include "keyboard.h"
#include "pit.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "io.h"
#include "panic.h"
#include "idt.h"

#define LINE_MAX 128
#define HIST_N   8
#define MAX_ARGS 8

static char history[HIST_N][LINE_MAX];
static int  hist_count;

static void prompt(void) {
    console_color(VGA_LGREEN); kprintf("ultralight");
    console_color(VGA_LGREY);  kprintf(":");
    console_color(VGA_LCYAN);  kprintf("~");
    console_color(VGA_LGREY);  kprintf("$ ");
}

static void erase_line(int len) { while (len-- > 0) kprintf("\b \b"); }

static void readline(char *buf, int cap) {
    int len = 0, hpos = hist_count;
    buf[0] = 0;
    for (;;) {
        int c = console_getc();
        if (c == '\n') { kprintf("\n"); buf[len] = 0; return; }
        if (c == '\b') { if (len > 0) { len--; erase_line(1); } continue; }
        if (c == 3) { kprintf("^C\n"); buf[0] = 0; return; }                 /* Ctrl+C */
        if (c == 21) { erase_line(len); len = 0; continue; }                 /* Ctrl+U */
        if (c == 12) { console_clear(); prompt(); for (int i = 0; i < len; i++) console_putc(buf[i]); continue; } /* Ctrl+L */
        if (c == KEY_UP || c == KEY_DOWN) {
            int target = hpos + (c == KEY_UP ? -1 : 1);
            if (target < 0 || target > hist_count || hist_count == 0) continue;
            if (hist_count > HIST_N && target < hist_count - HIST_N) continue;
            erase_line(len);
            hpos = target;
            if (hpos == hist_count) { len = 0; }
            else { strncpy(buf, history[hpos % HIST_N], cap - 1); buf[cap - 1] = 0; len = (int)strlen(buf); }
            for (int i = 0; i < len; i++) console_putc(buf[i]);
            continue;
        }
        if (c >= 32 && c < 127 && len < cap - 1) { buf[len++] = (char)c; console_putc((char)c); }
    }
}

static int tokenize(char *line, char **argv) {
    int argc = 0;
    while (*line && argc < MAX_ARGS) {
        while (*line == ' ' || *line == '\t') *line++ = 0;
        if (!*line) break;
        argv[argc++] = line;
        while (*line && *line != ' ' && *line != '\t') line++;
    }
    return argc;
}

/* ---- commands ---- */
static void cmd_help(int argc, char **argv);

static void cmd_clear(int argc, char **argv) { (void)argc; (void)argv; console_clear(); }

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) kprintf("%s%s", i > 1 ? " " : "", argv[i]);
    kprintf("\n");
}

static void cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint64_t t = pit_ticks(), s = t / PIT_HZ;
    kprintf("up %llu:%02llu:%02llu (%llu ticks)\n", (unsigned long long)(s / 3600),
            (unsigned long long)(s / 60 % 60), (unsigned long long)(s % 60), (unsigned long long)t);
}

static void cmd_mem(int argc, char **argv) {
    (void)argc; (void)argv;
    heap_stats_t h; heap_stats(&h);
    unsigned long long tot = pmm_total_frames(), fr = pmm_free_frames();
    kprintf("Physical: %llu KiB total, %llu KiB used, %llu KiB free (%llu frames free)\n",
            tot * 4, (tot - fr) * 4, fr * 4, fr);
    kprintf("Heap:     %llu KiB mapped, %llu B used, %llu B free, %llu blocks\n",
            (unsigned long long)h.heap_size / 1024, (unsigned long long)h.used,
            (unsigned long long)h.free, (unsigned long long)h.blocks);
    kprintf("PML4 at   %#llx\n", (unsigned long long)vmm_kernel_pml4());
}

static void cmd_memtest(int argc, char **argv) {
    (void)argc; (void)argv;
    enum { N = 64 };
    uint8_t *p[N];
    heap_stats_t before, after; heap_stats(&before);
    for (int i = 0; i < N; i++) {
        size_t sz = 16 + (size_t)(i * 37) % 3000;
        p[i] = kmalloc(sz);
        if (!p[i]) { kprintf("FAIL: kmalloc(%zu) returned NULL\n", sz); return; }
        memset(p[i], i + 1, sz);
        p[i][0] = (uint8_t)(i + 1);
    }
    for (int i = 0; i < N; i++) {
        size_t sz = 16 + (size_t)(i * 37) % 3000;
        for (size_t j = 0; j < sz; j++)
            if (p[i][j] != (uint8_t)(i + 1)) { kprintf("FAIL: corruption in block %d at +%zu\n", i, j); return; }
    }
    for (int i = 0; i < N; i += 2) kfree(p[i]);
    for (int i = 1; i < N; i += 2) kfree(p[i]);
    void *big = kmalloc(200 * 1024);                       /* forces heap growth */
    if (!big) { kprintf("FAIL: big allocation\n"); return; }
    memset(big, 0xAB, 200 * 1024);
    kfree(big);
    heap_stats(&after);
    kprintf("kmalloc/kfree: %d blocks verified; heap %llu -> %llu KiB; used bytes after: %llu\n", N,
            (unsigned long long)before.heap_size / 1024, (unsigned long long)after.heap_size / 1024,
            (unsigned long long)after.used);

    uint64_t a = pmm_alloc(), b = pmm_alloc();
    kprintf("pmm_alloc: %#llx, %#llx  ", (unsigned long long)a, (unsigned long long)b);
    pmm_free(a); pmm_free(b);
    kprintf("(freed)\nvmm_translate(heap base) = %#llx\n", (unsigned long long)vmm_translate(0x0000008000000000ULL));
    kprintf("memtest PASSED\n");
}

static void cmd_cpuid(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t a, b, c, d; char v[13];
    cpuid(0, &a, &b, &c, &d);
    memcpy(v, &b, 4); memcpy(v + 4, &d, 4); memcpy(v + 8, &c, 4); v[12] = 0;
    kprintf("CPU vendor: %s\n", v);
    char name[49]; name[48] = 0;
    cpuid(0x80000000, &a, &b, &c, &d);
    if (a >= 0x80000004) {
        for (uint32_t i = 0; i < 3; i++) { cpuid(0x80000002 + i, &a, &b, &c, &d);
            memcpy(name + i * 16, &a, 4); memcpy(name + i * 16 + 4, &b, 4);
            memcpy(name + i * 16 + 8, &c, 4); memcpy(name + i * 16 + 12, &d, 4); }
        kprintf("CPU brand:  %s\n", name);
    }
}

static void cmd_keytest(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("Key test: press keys (ESC to quit). Shows each key code.\n");
    for (;;) {
        int c = console_getc();
        if (c == 27) break;
        if (c >= 32 && c < 127) kprintf("'%c' = %d\n", c, c); else kprintf("code %d (%#x)\n", c, c);
    }
}

static void cmd_sleep(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: sleep <ms>\n"); return; }
    bool ok; unsigned long long ms = kstrtoull(argv[1], 10, &ok);
    if (!ok) { kprintf("sleep: bad number\n"); return; }
    pit_sleep_ms((uint32_t)ms);
}

static void cmd_peek(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: peek <hexaddr> (reads 8 bytes; translates first)\n"); return; }
    bool ok; uint64_t a = kstrtoull(argv[1], 16, &ok);
    if (!ok) { kprintf("peek: bad address\n"); return; }
    if (!vmm_translate(a)) { kprintf("%#llx is not mapped\n", (unsigned long long)a); return; }
    kprintf("%#llx: ", (unsigned long long)a);
    for (int i = 0; i < 8; i++) kprintf("%02x ", *(volatile uint8_t *)(a + (uint64_t)i));
    kprintf("\n");
}

static void cmd_crash(int argc, char **argv) {
    const char *w = argc > 1 ? argv[1] : "";
    if (!strcmp(w, "div0"))      { volatile int z = 0; kprintf("%d\n", 1 / z); }
    else if (!strcmp(w, "null")) { *(volatile int *)0 = 1; }
    else if (!strcmp(w, "ud"))   { __asm__ volatile("ud2"); }
    else if (!strcmp(w, "wp"))   { extern char _text_start[]; *(volatile uint8_t *)_text_start = 0; }  /* write to read-only text */
    else if (!strcmp(w, "panic")) { panic("manual panic from shell"); }
    else kprintf("usage: crash div0|null|ud|wp|panic   (deliberately triggers a fault)\n");
}

static void cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    cpu_cli();
    for (int i = 0; i < 100000; i++) if (!(inb(0x64) & 2)) break;
    outb(0x64, 0xFE);                                       /* pulse CPU reset line via keyboard controller */
    for (;;) cpu_hlt();
}

static void cmd_halt(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("System halted. You may close the window.\n");
    outw(0x604, 0x2000);                                    /* QEMU ACPI shutdown */
    outw(0xB004, 0x2000);                                   /* older QEMU / Bochs */
    cpu_cli();
    for (;;) cpu_hlt();
}

static const struct { const char *name, *help; void (*fn)(int, char **); } commands[] = {
    { "help",    "list commands",                       cmd_help },
    { "clear",   "clear the screen",                    cmd_clear },
    { "echo",    "print arguments",                     cmd_echo },
    { "uptime",  "time since boot",                     cmd_uptime },
    { "mem",     "physical memory + heap statistics",   cmd_mem },
    { "memtest", "stress-test PMM, VMM and kmalloc",    cmd_memtest },
    { "cpuid",   "show CPU vendor and brand",           cmd_cpuid },
    { "keytest", "show key codes (ESC quits)",          cmd_keytest },
    { "sleep",   "sleep <ms>",                          cmd_sleep },
    { "peek",    "peek <hexaddr>: read 8 bytes",        cmd_peek },
    { "crash",   "crash div0|null|ud|wp|panic",         cmd_crash },
    { "reboot",  "reset the machine",                   cmd_reboot },
    { "halt",    "power off (QEMU) / halt",             cmd_halt },
};

static void cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    for (size_t i = 0; i < ARRAY_SIZE(commands); i++) kprintf("  %-8s %s\n", commands[i].name, commands[i].help);
    kprintf("Editing: Backspace, Ctrl+U clear line, Ctrl+L redraw, Up/Down history, Ctrl+C cancel.\n");
}

void shell_run(void) {
    char line[LINE_MAX], *argv[MAX_ARGS];
    for (;;) {
        prompt();
        readline(line, sizeof line);
        if (!line[0]) continue;
        strncpy(history[hist_count % HIST_N], line, LINE_MAX - 1);
        history[hist_count % HIST_N][LINE_MAX - 1] = 0;
        hist_count++;

        int argc = tokenize(line, argv);
        if (!argc) continue;
        bool found = false;
        for (size_t i = 0; i < ARRAY_SIZE(commands); i++)
            if (!strcmp(argv[0], commands[i].name)) { commands[i].fn(argc, argv); found = true; break; }
        if (!found) kprintf("%s: command not found (try 'help')\n", argv[0]);
    }
}
