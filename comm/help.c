/* comm/help.c - list all registered commands */
#include "comm.h"
#include "kprintf.h"

static void cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    for (const command_t *c = comm_begin(); c < comm_end(); c++)
        kprintf("  %-10s %s\n", c->name, c->help);
    kprintf("Editing: Backspace, Ctrl+U clear line, Ctrl+L redraw, Up/Down history, Ctrl+C cancel.\n");
}
COMMAND("help", "list commands", cmd_help);
