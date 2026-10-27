/* kernel/kernel.c - UltraLight kernel entry (Phase 1 + 2) */
#include "types.h"
#include "io.h"
#include "console.h"
#include "serial.h"
#include "kprintf.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "multiboot.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "panic.h"
#include "version.h"
#include "shell.h"

static bootinfo_t bootinfo;

static void ok(const char *what) {
    console_color(VGA_LGREEN); kprintf("[ OK ] ");
    console_color(VGA_LGREY);  kprintf("%s\n", what);
}

void kernel_main(uint64_t magic, uint64_t info);
void kernel_main(uint64_t magic, uint64_t info) {
    serial_init();
    vga_init();
    console_color(VGA_LCYAN);
    kprintf("UltraLight OS v%s (x86_64)\n", UL_VERSION);
    console_color(VGA_LGREY);
    kprintf("---------------------------\n");

    gdt_init();                         ok("GDT + TSS loaded");
    idt_init();                         ok("IDT: 32 exceptions + 16 IRQs");
    pic_init();                         ok("8259 PIC remapped to vectors 32-47");

    multiboot_parse((uint32_t)magic, info, &bootinfo);
    kprintf("       bootloader: %s (multiboot%d)\n", bootinfo.loader[0] ? bootinfo.loader : "qemu/unknown", bootinfo.protocol);

    pmm_init(&bootinfo);
    kprintf("[ OK ] PMM: %llu MiB usable, %llu frames free\n",
            (unsigned long long)(pmm_total_frames() * PAGE_SIZE >> 20), (unsigned long long)pmm_free_frames());

    vmm_init();                         ok("VMM: 4-level paging active (W^X kernel, NULL page unmapped)");
    heap_init();                        ok("Heap: kmalloc/kfree ready");

    pit_init(PIT_HZ);                   ok("PIT timer @ 100 Hz");
    keyboard_init();                    ok("PS/2 keyboard driver");

    cpu_sti();
    console_color(VGA_LGREY);
    kprintf("\nType 'help' for commands.\n\n");
    shell_run();
}
