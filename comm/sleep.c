/* comm/sleep.c - sleep for N milliseconds */
#include "comm.h"
#include "kprintf.h"
#include "pit.h"
#include "string.h"

static void cmd_sleep(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: sleep <ms>\n"); return; }
    bool ok; unsigned long long ms = kstrtoull(argv[1], 10, &ok);
    if (!ok) { kprintf("sleep: bad number\n"); return; }
    pit_sleep_ms((uint32_t)ms);
}
COMMAND("sleep", "sleep <ms>", cmd_sleep);
