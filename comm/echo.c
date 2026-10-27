/* comm/echo.c - print arguments */
#include "comm.h"
#include "kprintf.h"

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) kprintf("%s%s", i > 1 ? " " : "", argv[i]);
    kprintf("\n");
}
COMMAND("echo", "print arguments", cmd_echo);
