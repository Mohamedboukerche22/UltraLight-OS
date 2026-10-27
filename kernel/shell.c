/* kernel/shell.c - kernel monitor: line editor + command dispatch
 *
 * Commands are NOT defined here.  Every command lives under comm/ and
 * self-registers via COMMAND(); see include/comm.h.  To add a command,
 * create comm/<name>.c - no changes to this file are required.
 */
#include "shell.h"
#include "comm.h"
#include "console.h"
#include "kprintf.h"
#include "string.h"
#include "keyboard.h"

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

        const command_t *cmd = comm_find(argv[0]);
        if (cmd) cmd->fn(argc, argv);
        else kprintf("%s: command not found (try 'help')\n", argv[0]);
    }
}
