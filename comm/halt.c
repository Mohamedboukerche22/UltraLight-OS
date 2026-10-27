/* comm/halt.c - power off (QEMU) or halt */
#include "comm.h"
#include "kprintf.h"
#include "io.h"

static void cmd_halt(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("System halted. You may close the window.\n");
    outw(0x604, 0x2000);                                    /* QEMU ACPI shutdown */
    outw(0xB004, 0x2000);                                   /* older QEMU / Bochs */
    cpu_cli();
    for (;;) cpu_hlt();
}
COMMAND("halt", "power off (QEMU) / halt", cmd_halt);
