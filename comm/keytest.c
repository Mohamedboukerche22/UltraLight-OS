/* comm/keytest.c - dump key codes until ESC */
#include "comm.h"
#include "console.h"
#include "kprintf.h"

static void cmd_keytest(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("Key test: press keys (ESC to quit). Shows each key code.\n");
    for (;;) {
        int c = console_getc();
        if (c == 27) break;
        if (c >= 32 && c < 127) kprintf("'%c' = %d\n", c, c); else kprintf("code %d (%#x)\n", c, c);
    }
}
COMMAND("keytest", "show key codes (ESC quits)", cmd_keytest);
