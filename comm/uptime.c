/* comm/uptime.c - time since boot from the PIT tick counter */
#include "comm.h"
#include "kprintf.h"
#include "pit.h"

static void cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint64_t t = pit_ticks(), s = t / PIT_HZ;
    kprintf("up %llu:%02llu:%02llu (%llu ticks)\n", (unsigned long long)(s / 3600),
            (unsigned long long)(s / 60 % 60), (unsigned long long)(s % 60),
            (unsigned long long)t);
}
COMMAND("uptime", "time since boot", cmd_uptime);
