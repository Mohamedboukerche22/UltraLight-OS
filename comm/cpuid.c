/* comm/cpuid.c - show CPU vendor and brand strings */
#include "comm.h"
#include "kprintf.h"
#include "string.h"
#include "io.h"

static void cmd_cpuid(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t a, b, c, d; char v[13];
    cpuid(0, &a, &b, &c, &d);
    memcpy(v, &b, 4); memcpy(v + 4, &d, 4); memcpy(v + 8, &c, 4); v[12] = 0;
    kprintf("CPU vendor: %s\n", v);
    char name[49]; name[48] = 0;
    cpuid(0x80000000, &a, &b, &c, &d);
    if (a >= 0x80000004) {
        for (uint32_t i = 0; i < 3; i++) { cpuid(0x80000002 + i, &a, &b, &c, &d);
            memcpy(name + i * 16, &a, 4); memcpy(name + i * 16 + 4, &b, 4);
            memcpy(name + i * 16 + 8, &c, 4); memcpy(name + i * 16 + 12, &d, 4); }
        kprintf("CPU brand:  %s\n", name);
    }
}
COMMAND("cpuid", "show CPU vendor and brand", cmd_cpuid);
