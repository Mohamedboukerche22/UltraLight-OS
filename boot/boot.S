/* boot/boot.S - UltraLight entry point.
 *
 * The image carries BOTH a Multiboot1 and a Multiboot2 header:
 *   - Multiboot1: what `qemu -kernel` understands (QEMU does not speak MB2).
 *   - Multiboot2: what GRUB uses when booting the ISO.
 * The bootloader enters in 32-bit protected mode (EAX = magic, EBX = info ptr).
 * We build identity-mapped page tables for the first 4 GiB, enter long mode
 * and call kernel_main(magic, info) with the SysV ABI (RDI, RSI).
 */
#define MB1_MAGIC  0x1BADB002
#define MB1_FLAGS  0x00000003          /* bit0: page-align modules, bit1: want memory map */
#define MB2_MAGIC  0xE85250D6

.section .multiboot, "a"
.align 4
mb1_header:
    .long MB1_MAGIC
    .long MB1_FLAGS
    .long 0x100000000 - (MB1_MAGIC + MB1_FLAGS)

.align 8
mb2_header:
    .long MB2_MAGIC
    .long 0                                    /* architecture: i386 */
    .long mb2_header_end - mb2_header
    .long 0x100000000 - (MB2_MAGIC + (mb2_header_end - mb2_header))
    .short 0                                   /* end tag */
    .short 0
    .long 8
mb2_header_end:

/* ------------------------------------------------------------------ */
.section .boot_data, "aw"
.align 8
mb_magic: .long 0
mb_info:  .long 0

.align 16
boot_gdt:
    .quad 0x0000000000000000      /* null */
    .quad 0x00209A0000000000      /* 0x08: 64-bit code */
    .quad 0x0000920000000000      /* 0x10: data */
boot_gdt_end:
boot_gdt_ptr:
    .short boot_gdt_end - boot_gdt - 1
    .long  boot_gdt

msg_no_lm: .asciz "UltraLight: this CPU does not support 64-bit mode (run qemu-system-x86_64)"

/* ------------------------------------------------------------------ */
.section .boot, "ax"
.code32
.global _start
_start:
    cli
    movl $boot_stack_top, %esp
    movl %eax, mb_magic
    movl %ebx, mb_info

    /* Is long mode available? */
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb   no_long_mode
    movl $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx
    jz   no_long_mode

    /* Zero the six page-table pages: PML4, PDPT, 4x PD */
    xorl %eax, %eax
    movl $boot_pml4, %edi
    movl $(6 * 1024), %ecx
    rep stosl

    /* PML4[0] -> PDPT */
    movl $boot_pdpt, %eax
    orl  $0x3, %eax
    movl %eax, boot_pml4

    /* PDPT[0..3] -> PD0..PD3 */
    xorl %ecx, %ecx
1:  movl %ecx, %eax
    shll $12, %eax
    addl $boot_pd, %eax
    orl  $0x3, %eax
    movl %eax, boot_pdpt(, %ecx, 8)
    incl %ecx
    cmpl $4, %ecx
    jne  1b

    /* 2048 x 2 MiB pages = identity map of 4 GiB (P | W | PS) */
    xorl %ecx, %ecx
2:  movl %ecx, %eax
    shll $21, %eax
    orl  $0x83, %eax
    movl %eax, boot_pd(, %ecx, 8)
    incl %ecx
    cmpl $2048, %ecx
    jne  2b

    movl %cr4, %eax              /* PAE */
    orl  $0x20, %eax
    movl %eax, %cr4

    movl $boot_pml4, %eax
    movl %eax, %cr3

    movl $0xC0000080, %ecx       /* EFER: LME | NXE */
    rdmsr
    orl  $0x900, %eax
    wrmsr

    movl %cr0, %eax              /* PG | WP | PE */
    orl  $0x80010001, %eax
    movl %eax, %cr0

    lgdt boot_gdt_ptr
    ljmp $0x08, $long_mode_start

no_long_mode:
    movl $msg_no_lm, %esi
    movl $0xB8000, %edi
    movb $0x4F, %ah              /* white on red */
3:  lodsb
    testb %al, %al
    jz   4f
    movw %ax, (%edi)
    addl $2, %edi
    jmp  3b
4:  cli
    hlt
    jmp  4b

/* ------------------------------------------------------------------ */
.code64
long_mode_start:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    xorl %eax, %eax
    movw %ax, %fs
    movw %ax, %gs

    movq $boot_stack_top, %rsp
    xorl %ebp, %ebp
    movl mb_magic(%rip), %edi
    movl mb_info(%rip), %esi
    call kernel_main

    cli
5:  hlt
    jmp 5b

/* ------------------------------------------------------------------ */
.section .bss
.align 4096
boot_pml4: .skip 4096
boot_pdpt: .skip 4096
boot_pd:   .skip 4096 * 4

.align 16
boot_stack_bottom: .skip 65536
.global boot_stack_top
boot_stack_top:
