# UltraLight OS — Phase 1 + 2 (x86_64)

A from-scratch, minimalist x86_64 kernel. Boots via Multiboot1 (QEMU `-kernel`) or Multiboot2 (GRUB ISO).

## Run
```
sudo apt install build-essential qemu-system-x86      # Debian/Ubuntu
./run.sh              # QEMU window — click it and type
./run.sh --serial     # no window; use this terminal (Ctrl-A then X quits)
./run.sh --iso        # boot through GRUB / Multiboot2   (needs: grub-pc-bin xorriso mtools)
./run.sh --debug      # paused, gdb stub on :1234, interrupt log in qemu.log
```
Use `qemu-system-x86_64` (not `-i386`). Build output is never hidden.

## What's implemented
| Area | Files |
|---|---|
| Boot: MB1+MB2 headers, long-mode switch, 4 GiB identity map | `boot/boot.S`, `linker.ld` |
| GDT + TSS (IST1 for #DF), IDT, 32 exception + 16 IRQ stubs, PIC, PIT | `arch/x86_64/*` , `drivers/pit.c` |
| Drivers: VGA text, COM1 serial, **PS/2 keyboard** (shift/caps/ctrl/arrows) | `drivers/*` |
| Unified console (VGA+serial out; keyboard+serial in) | `kernel/console.c` |
| PMM: bitmap frame allocator from the Multiboot memory map | `mm/pmm.c` |
| VMM: 4-level paging, W^X kernel mapping, NULL page unmapped, `vmm_map/alloc/unmap/translate` | `mm/vmm.c` |
| Heap: `kmalloc/kcalloc/krealloc/kfree`, split+coalesce, grows on demand at 512 GiB | `mm/heap.c` |
| Kernel shell | `kernel/shell.c` |

## Shell commands
`ls help clear echo uptime mem memtest cpuid keytest sleep peek crash reboot halt`
(`ls [prefix]` lists commands; `help` shows what each one does.
`crash div0|null|ud|wp|panic` deliberately triggers CPU exceptions to test the handlers.)

## Adding a command
Commands live in `comm/`, one file per command, and register themselves into a
linker section. To add one, drop a new `comm/<name>.c` in place:

```c
#include "comm.h"
#include "kprintf.h"

static void cmd_hello(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("hello from comm/hello.c\n");
}
COMMAND("hello", "say hello", cmd_hello);
```

That is the whole wiring: the Makefile wildcard compiles `comm/*.c`, the linker
collects `.comm_commands`, and `kernel/shell.c` dispatches through the registry.
No kernel, shell, Makefile or registry edit is needed, so a new command cannot
break the rest of the kernel.

## Layout
```
boot/  arch/x86_64/  drivers/  kernel/  mm/  lib/  comm/  include/  linker.ld  Makefile  run.sh
```

Next: Phase 3 (PCB, scheduler, syscalls) — TSS/GDT already contain the user segments it needs.
