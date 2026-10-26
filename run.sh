#!/usr/bin/env bash
# UltraLight launcher.   ./run.sh [--serial | --iso | --debug | --clean]
#   (default)  QEMU window: type in the window (PS/2 keyboard); serial log goes to this terminal
#   --serial   no window; everything runs in this terminal (Ctrl-A then X quits QEMU)
#   --iso      build a GRUB (Multiboot2) ISO and boot that instead of qemu -kernel
#   --debug    start paused with a GDB stub on :1234 and an interrupt log in qemu.log
set -euo pipefail
cd "$(dirname "$0")"

need() { command -v "$1" >/dev/null 2>&1 || { echo "error: '$1' not found. Install: $2" >&2; exit 1; }; }
need gcc  "sudo apt install build-essential"
need make "sudo apt install build-essential"
need qemu-system-x86_64 "sudo apt install qemu-system-x86"

if [ "$(uname -m)" != "x86_64" ]; then
    echo "error: host is $(uname -m); build with an x86_64 cross toolchain, e.g. make CC=x86_64-elf-gcc LD=x86_64-elf-ld OBJCOPY=x86_64-elf-objcopy" >&2
    exit 1
fi

mode="${1:-}"
[ "$mode" = "--clean" ] && { make clean; exit 0; }

make -j"$(nproc 2>/dev/null || echo 2)"      # build errors are shown, never hidden

case "$mode" in
    --serial) exec make run-serial ;;
    --iso)    exec make run-iso ;;
    --debug)  echo "Attach with: gdb build/ultralight64.elf -ex 'target remote :1234'"; exec make debug ;;
    "")       exec make run ;;
    *)        echo "unknown option: $mode"; exit 1 ;;
esac
