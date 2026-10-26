/* arch/x86_64/idt.c - IDT, exception reporting, IRQ dispatch */
#include "idt.h"
#include "gdt.h"
#include "pic.h"
#include "io.h"
#include "kprintf.h"
#include "panic.h"
#include "console.h"
#include "string.h"

struct idt_entry {
    uint16_t off_lo;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  attr;
    uint16_t off_mid;
    uint32_t off_hi;
    uint32_t zero;
} PACKED;

static struct idt_entry idt[256];
static isr_handler_t handlers[256];
extern uint64_t isr_stub_table[48];

static const char *exc_names[32] = {
    "Divide Error", "Debug", "Non-Maskable Interrupt", "Breakpoint", "Overflow",
    "Bound Range Exceeded", "Invalid Opcode", "Device Not Available", "Double Fault",
    "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present", "Stack-Segment Fault",
    "General Protection Fault", "Page Fault", "Reserved", "x87 FPU Error", "Alignment Check",
    "Machine Check", "SIMD FP Exception", "Virtualization Exception", "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved"
};

static void set_gate(int n, uint64_t handler, uint8_t ist) {
    idt[n].off_lo   = handler & 0xFFFF;
    idt[n].selector = GDT_KCODE;
    idt[n].ist      = ist;
    idt[n].attr     = 0x8E;                    /* present, DPL0, 64-bit interrupt gate */
    idt[n].off_mid  = (handler >> 16) & 0xFFFF;
    idt[n].off_hi   = (uint32_t)(handler >> 32);
    idt[n].zero     = 0;
}

void idt_set_handler(uint8_t vector, isr_handler_t h) { handlers[vector] = h; }

void idt_init(void) {
    memset(idt, 0, sizeof idt);
    for (int i = 0; i < 48; i++)
        set_gate(i, isr_stub_table[i], i == 8 ? 1 : 0);   /* #DF runs on IST1 */

    struct { uint16_t limit; uint64_t base; } PACKED idtr = { sizeof idt - 1, (uint64_t)idt };
    __asm__ volatile("lidt %0" : : "m"(idtr));
}

static void exception_panic(struct regs *r) {
    cpu_cli();
    console_color(VGA_WHITE);
    kprintf("\n*** KERNEL EXCEPTION %llu: %s ***\n", (unsigned long long)r->int_no,
            r->int_no < 32 ? exc_names[r->int_no] : "?");
    kprintf("err=%#llx  rip=%#llx  cs=%#llx  rflags=%#llx\n",
            (unsigned long long)r->err_code, (unsigned long long)r->rip,
            (unsigned long long)r->cs, (unsigned long long)r->rflags);
    kprintf("rax=%016llx rbx=%016llx rcx=%016llx\nrdx=%016llx rsi=%016llx rdi=%016llx\n",
            (unsigned long long)r->rax, (unsigned long long)r->rbx, (unsigned long long)r->rcx,
            (unsigned long long)r->rdx, (unsigned long long)r->rsi, (unsigned long long)r->rdi);
    kprintf("rbp=%016llx rsp=%016llx\n", (unsigned long long)r->rbp, (unsigned long long)r->rsp);
    if (r->int_no == 14) {
        kprintf("CR2 (faulting address) = %#llx  [%s %s%s%s]\n", (unsigned long long)read_cr2(),
                (r->err_code & 1) ? "protection" : "not-present",
                (r->err_code & 2) ? "write" : "read",
                (r->err_code & 4) ? " user" : " kernel",
                (r->err_code & 16) ? " ifetch" : "");
    }
    panic("unhandled CPU exception");
}

void isr_dispatch(struct regs *r);
void isr_dispatch(struct regs *r) {
    uint64_t n = r->int_no;

    if (n >= IRQ_BASE && n < IRQ_BASE + 16) {
        uint8_t irq = (uint8_t)(n - IRQ_BASE);
        if (pic_is_spurious(irq)) return;      /* spurious: no handler, no EOI (except master for IRQ15) */
        if (handlers[n]) handlers[n](r);
        pic_eoi(irq);
        return;
    }

    if (handlers[n]) { handlers[n](r); return; }
    if (n < 32) exception_panic(r);
}
