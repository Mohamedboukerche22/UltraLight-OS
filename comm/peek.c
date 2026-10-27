/* comm/peek.c - read 8 bytes from a mapped virtual address */
#include "comm.h"
#include "kprintf.h"
#include "vmm.h"
#include "string.h"

static void cmd_peek(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: peek <hexaddr> (reads 8 bytes; translates first)\n"); return; }
    bool ok; uint64_t a = kstrtoull(argv[1], 16, &ok);
    if (!ok) { kprintf("peek: bad address\n"); return; }
    if (!vmm_translate(a)) { kprintf("%#llx is not mapped\n", (unsigned long long)a); return; }
    kprintf("%#llx: ", (unsigned long long)a);
    for (int i = 0; i < 8; i++) kprintf("%02x ", *(volatile uint8_t *)(a + (uint64_t)i));
    kprintf("\n");
}
COMMAND("peek", "peek <hexaddr>: read 8 bytes", cmd_peek);
