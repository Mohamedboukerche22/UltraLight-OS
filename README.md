# UltraLight OS — Phase 1 + 2 (x86_64)

A from-scratch, minimalist x86_64 kernel. Boots via Multiboot1 (QEMU `-kernel`) or Multiboot2 (BIOS ISO via Limine).

## Run
```
sudo apt install build-essential qemu-system-x86      # Debian/Ubuntu
sudo pacman -S base-devel qemu-full limine            # Arch (limine is only for `make iso`)
./run.sh              # QEMU window — click it and type
./run.sh --serial     # no window; use this terminal (Ctrl-A then X quits)
./run.sh --iso        # build a BIOS ISO and boot it (Limine, no grub/xorriso needed)
./run.sh --debug      # paused, gdb stub on :1234, interrupt log in qemu.log
```
Use `qemu-system-x86_64` (not `-i386`). Build output is never hidden.

## Bootable ISO (old BIOS PCs)
```
make iso
qemu-system-x86_64 -cdrom build/ultralight.iso -m 128M -serial stdio
```
`build/ultralight.iso` is a plain ISO9660 + El Torito BIOS image. Burn it to a
CD/DVD, or let Ventoy/Rufus write it to a USB stick. It does not need GRUB,
xorriso, mtools or root privileges to build: `tools/mkiso.py` creates the ISO
and Limine provides the BIOS bootloader. If Limine lives somewhere else, pass
`make iso LIMINE_DIR=/path/to/share/limine`.

## What's implemented
| Area | Files |
|---|---|
| Boot: MB1+MB2 headers, long-mode switch, 4 GiB identity map | `boot/boot.S`, `linker.ld` |
| BIOS ISO boot: Limine + dependency-free ISO9660/El Torito builder | `boot/limine.conf`, `tools/mkiso.py` |
| GDT + TSS (IST1 for #DF), IDT, 32 exception + 16 IRQ stubs, PIC, PIT | `arch/x86_64/*` , `drivers/pit.c` |
| Drivers: VGA text, COM1 serial, **PS/2 keyboard** (shift/caps/ctrl/arrows) | `drivers/*` |
| Unified console (VGA+serial out; keyboard+serial in) | `kernel/console.c` |
| PMM: bitmap frame allocator from the Multiboot memory map | `mm/pmm.c` |
| VMM: 4-level paging, W^X kernel mapping, NULL page unmapped, `vmm_map/alloc/unmap/translate` | `mm/vmm.c` |
| Heap: `kmalloc/kcalloc/krealloc/kfree`, split+coalesce, grows on demand at 512 GiB | `mm/heap.c` |
| Kernel shell + neofetch-style `mohfetch` system summary | `kernel/shell.c`, `comm/mohfetch.c` |

## Shell commands
`mohfetch fetch ls help clear echo uptime mem memtest cpuid keytest sleep peek crash reboot halt`
(`mohfetch` prints an ASCII logo with OS, CPU, memory, uptime and bootloader info.
`ls [prefix]` lists commands; `help` shows what each one does.
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
boot/  arch/x86_64/  drivers/  kernel/  mm/  lib/  comm/  include/  tools/  linker.ld  Makefile  run.sh
```

Next: Phase 3 (PCB, scheduler, syscalls) — TSS/GDT already contain the user segments it needs.
