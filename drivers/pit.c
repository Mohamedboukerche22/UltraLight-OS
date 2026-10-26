/* drivers/pit.c - 8253/8254 programmable interval timer, IRQ0 */
#include "pit.h"
#include "idt.h"
#include "pic.h"
#include "io.h"

static volatile uint64_t ticks;
static uint32_t tick_hz = PIT_HZ;

static void pit_irq(struct regs *r) { (void)r; ticks++; }

void pit_init(uint32_t hz) {
    tick_hz = hz;
    uint32_t div = 1193182 / hz;
    outb(0x43, 0x36);                       /* channel 0, lobyte/hibyte, rate generator */
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
    idt_set_handler(IRQ_BASE + 0, pit_irq);
    pic_unmask(0);
}

uint64_t pit_ticks(void) { return ticks; }

void pit_sleep_ms(uint32_t ms) {
    uint64_t target = ticks + ((uint64_t)ms * tick_hz + 999) / 1000;
    while (ticks < target) cpu_idle();
}
