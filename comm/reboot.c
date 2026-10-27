/* comm/reboot.c - reset the machine via the keyboard controller */
#include "comm.h"
#include "io.h"

static void cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    cpu_cli();
    for (int i = 0; i < 100000; i++) if (!(inb(0x64) & 2)) break;
    outb(0x64, 0xFE);                                       /* pulse CPU reset line */
    for (;;) cpu_hlt();
}
COMMAND("reboot", "reset the machine", cmd_reboot);
