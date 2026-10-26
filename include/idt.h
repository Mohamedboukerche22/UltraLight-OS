#ifndef UL_IDT_H
#define UL_IDT_H
#include "types.h"

struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};
typedef void (*isr_handler_t)(struct regs *);

void idt_init(void);
void idt_set_handler(uint8_t vector, isr_handler_t h);
#define IRQ_BASE 32
#endif
