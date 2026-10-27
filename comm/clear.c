/* comm/clear.c - clear the VGA/serial console */
#include "comm.h"
#include "console.h"

static void cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    console_clear();
}
COMMAND("clear", "clear the screen", cmd_clear);
