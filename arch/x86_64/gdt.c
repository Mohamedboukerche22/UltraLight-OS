/* arch/x86_64/gdt.c - GDT with kernel/user segments and a TSS (for IST stacks) */
#include "gdt.h"
#include "string.h"

struct tss {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} PACKED;

static uint64_t gdt[7];                       /* null, kcode, kdata, udata, ucode, tss(2) */
static struct tss tss;
static uint8_t ist1_stack[16384] __attribute__((aligned(16)));   /* double-fault stack */

extern char boot_stack_top[];

void tss_set_rsp0(uint64_t rsp0) { tss.rsp[0] = rsp0; }

void gdt_init(void) {
    gdt[0] = 0;
    gdt[1] = 0x00AF9A000000FFFFULL;           /* 0x08 kernel code, L=1 */
    gdt[2] = 0x00CF92000000FFFFULL;           /* 0x10 kernel data */
    gdt[3] = 0x00CFF2000000FFFFULL;           /* 0x18 user data (DPL3) */
    gdt[4] = 0x00AFFA000000FFFFULL;           /* 0x20 user code (DPL3) */

    memset(&tss, 0, sizeof tss);
    tss.rsp[0]    = (uint64_t)boot_stack_top;
    tss.ist[0]    = (uint64_t)(ist1_stack + sizeof ist1_stack);
    tss.iomap_base = sizeof tss;

    uint64_t base = (uint64_t)&tss, limit = sizeof tss - 1;
    gdt[5] = (limit & 0xFFFF) | ((base & 0xFFFFFF) << 16) | (0x89ULL << 40) |
             (((limit >> 16) & 0xF) << 48) | (((base >> 24) & 0xFF) << 56);
    gdt[6] = base >> 32;

    struct { uint16_t limit; uint64_t base; } PACKED gdtr = { sizeof gdt - 1, (uint64_t)gdt };

    __asm__ volatile(
        "lgdt %0\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n"
        "1:\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%ss\n\t"
        "xorl %%eax, %%eax\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw $0x28, %%ax\n\t"
        "ltr %%ax\n\t"
        : : "m"(gdtr) : "rax", "memory");
}
