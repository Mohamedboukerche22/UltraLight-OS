# UltraLight OS - build with:  make        run with:  make run   (or ./run.sh)
CC      := gcc
LD      := ld
OBJCOPY := objcopy
BUILD   := build

CFLAGS := -std=gnu99 -ffreestanding -O2 -g -Wall -Wextra \
          -m64 -mno-red-zone -mgeneral-regs-only -mno-80387 \
          -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
          -fno-tree-loop-distribute-patterns -fcf-protection=none \
          -MMD -MP -Iinclude
ASFLAGS := -m64 -g -Iinclude
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -z noexecstack -T linker.ld

C_SRCS := $(wildcard arch/x86_64/*.c) $(wildcard drivers/*.c) $(wildcard kernel/*.c) \
          $(wildcard mm/*.c) $(wildcard lib/*.c) $(wildcard comm/*.c)
S_SRCS := boot/boot.S arch/x86_64/isr.S
OBJS   := $(patsubst %.c,$(BUILD)/%.o,$(C_SRCS)) $(patsubst %.S,$(BUILD)/%.o,$(S_SRCS))

KERNEL_ELF64 := $(BUILD)/ultralight64.elf
KERNEL_ELF32 := $(BUILD)/ultralight.elf      # same code, ELF32 wrapper so `qemu -kernel` accepts it
ISO          := $(BUILD)/ultralight.iso
LIMINE_DIR   ?= /usr/share/limine

QEMU     ?= qemu-system-x86_64
QEMUOPTS := -m 256M -serial stdio -no-reboot -no-shutdown

all: $(KERNEL_ELF32)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(KERNEL_ELF64): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(KERNEL_ELF32): $(KERNEL_ELF64)
	$(OBJCOPY) -O elf32-i386 $< $@

iso: $(ISO)
# Dependency-free BIOS/El Torito ISO: Limine stage files + tools/mkiso.py.
# No grub-mkrescue, xorriso, mtools or root access required.
$(ISO): $(KERNEL_ELF64) boot/limine.conf tools/mkiso.py
	@test -f $(LIMINE_DIR)/limine-bios-cd.bin || { \
		echo "Limine files missing from $(LIMINE_DIR)."; \
		echo "Install: sudo pacman -S limine   (Debian/Ubuntu: sudo apt install limine)"; \
		echo "Or build with LIMINE_DIR=/path/to/share/limine"; exit 1; }
	rm -rf $(BUILD)/isoroot && mkdir -p $(BUILD)/isoroot
	cp $(KERNEL_ELF64) $(BUILD)/isoroot/ultralight.elf
	cp boot/limine.conf $(BUILD)/isoroot/limine.conf
	cp $(LIMINE_DIR)/limine-bios.sys $(BUILD)/isoroot/
	cp $(LIMINE_DIR)/limine-bios-cd.bin $(BUILD)/isoroot/
	python3 tools/mkiso.py $(BUILD)/isoroot $@ limine-bios-cd.bin
	@echo "ISO ready: $@"
	@echo "BIOS test: $(QEMU) -cdrom $@ $(QEMUOPTS) -display none"

run: $(KERNEL_ELF32)
	$(QEMU) -kernel $(KERNEL_ELF32) $(QEMUOPTS)

run-serial: $(KERNEL_ELF32)
	$(QEMU) -kernel $(KERNEL_ELF32) -m 256M -nographic -no-reboot

run-iso: $(ISO)
	$(QEMU) -cdrom $(ISO) $(QEMUOPTS)

debug: $(KERNEL_ELF32)
	$(QEMU) -kernel $(KERNEL_ELF32) $(QEMUOPTS) -s -S -d int,cpu_reset -D qemu.log

clean:
	rm -rf $(BUILD) qemu.log

-include $(OBJS:.o=.d)

.PHONY: all iso run run-serial run-iso debug clean
