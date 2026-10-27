/* comm/ls.c - list registered commands, optionally filtered by prefix */
#include "comm.h"
#include "kprintf.h"
#include "string.h"

static void cmd_ls(int argc, char **argv) {
    const char *prefix = argc > 1 ? argv[1] : "";
    size_t plen = strlen(prefix);
    int col = 0;

    for (const command_t *c = comm_begin(); c < comm_end(); c++) {
        if (strncmp(c->name, prefix, plen) != 0) continue;
        kprintf("%-14s", c->name);
        if (++col % 5 == 0) kprintf("\n");
    }
    if (col % 5) kprintf("\n");
    if (!col) kprintf("ls: no command matches '%s'\n", prefix);
}
COMMAND("ls", "list commands [prefix]", cmd_ls);
