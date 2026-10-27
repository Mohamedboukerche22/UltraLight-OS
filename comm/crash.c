/* comm/crash.c - deliberately trigger CPU exceptions to test the handlers */
#include "comm.h"
#include "kprintf.h"
#include "string.h"
#include "panic.h"

static void cmd_crash(int argc, char **argv) {
    const char *w = argc > 1 ? argv[1] : "";
    if (!strcmp(w, "div0"))      { volatile int z = 0; kprintf("%d\n", 1 / z); }
    else if (!strcmp(w, "null")) { *(volatile int *)0 = 1; }
    else if (!strcmp(w, "ud"))   { __asm__ volatile("ud2"); }
    else if (!strcmp(w, "wp"))   { extern char _text_start[]; *(volatile uint8_t *)_text_start = 0; }
    else if (!strcmp(w, "panic")) { panic("manual panic from shell"); }
    else kprintf("usage: crash div0|null|ud|wp|panic   (deliberately triggers a fault)\n");
}
COMMAND("crash", "crash div0|null|ud|wp|panic", cmd_crash);
