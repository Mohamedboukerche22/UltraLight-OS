/* mm/vmm.c - 4-level paging (PML4 -> PDPT -> PD -> PT)
 *
 * Physical RAM is identity mapped (virt == phys) so page-table frames returned
 * by the PMM can be touched directly.  The kernel image itself is mapped with
 * 4K pages and W^X permissions (text = R-X, rodata = R--, data/bss = RW-).
 * Page 0 is left unmapped so NULL dereferences fault.
 */
#include "vmm.h"
#include "pmm.h"
#include "idt.h"
#include "io.h"
#include "string.h"
#include "panic.h"
#include "kprintf.h"

#define PTE_P    (1ULL << 0)
#define PTE_W    (1ULL << 1)
#define PTE_U    (1ULL << 2)
#define PTE_PWT  (1ULL << 3)
#define PTE_PCD  (1ULL << 4)
#define PTE_PS   (1ULL << 7)
#define PTE_NX   (1ULL << 63)
#define PTE_ADDR 0x000FFFFFFFFFF000ULL

extern char _kernel_start[], _text_start[], _text_end[], _rodata_start[], _rodata_end[], _kernel_end[];

static uint64_t *pml4;

static uint64_t make_pte(uint64_t phys, uint32_t f) {
    uint64_t e = phys | PTE_P;
    if (f & VMM_WRITE)   e |= PTE_W;
    if (f & VMM_USER)    e |= PTE_U;
    if (f & VMM_NOEXEC)  e |= PTE_NX;
    if (f & VMM_NOCACHE) e |= PTE_PCD | PTE_PWT;
    return e;
}

static uint64_t *new_table(void) {
    uint64_t p = pmm_alloc();
    if (!p) return NULL;
    memset((void *)p, 0, PAGE_SIZE);
    return (uint64_t *)p;
}

static uint64_t *next_level(uint64_t *tbl, unsigned idx, bool create, bool user) {
    uint64_t e = tbl[idx];
    if (e & PTE_P) {
        if (e & PTE_PS) return NULL;                 /* a huge page is in the way */
        if (user && !(e & PTE_U)) tbl[idx] = e | PTE_U;
        return (uint64_t *)(e & PTE_ADDR);
    }
    if (!create) return NULL;
    uint64_t *t = new_table();
    if (!t) return NULL;
    tbl[idx] = (uint64_t)t | PTE_P | PTE_W | (user ? PTE_U : 0);
    return t;
}

#define IDX4(v) (((v) >> 39) & 511)
#define IDX3(v) (((v) >> 30) & 511)
#define IDX2(v) (((v) >> 21) & 511)
#define IDX1(v) (((v) >> 12) & 511)

static uint64_t *walk(uint64_t v, bool create, bool user) {
    uint64_t *t3 = next_level(pml4, IDX4(v), create, user);  if (!t3) return NULL;
    uint64_t *t2 = next_level(t3, IDX3(v), create, user);    if (!t2) return NULL;
    uint64_t *t1 = next_level(t2, IDX2(v), create, user);    if (!t1) return NULL;
    return &t1[IDX1(v)];
}

int vmm_map(uint64_t virt, uint64_t phys, uint32_t flags) {
    uint64_t *pte = walk(virt, true, flags & VMM_USER);
    if (!pte) return -1;
    *pte = make_pte(phys, flags);
    invlpg(virt);
    return 0;
}

int vmm_alloc(uint64_t virt, size_t npages, uint32_t flags) {
    for (size_t i = 0; i < npages; i++) {
        uint64_t frame = pmm_alloc();
        if (!frame) return -1;
        memset((void *)frame, 0, PAGE_SIZE);
        if (vmm_map(virt + i * PAGE_SIZE, frame, flags) < 0) { pmm_free(frame); return -1; }
    }
    return 0;
}

void vmm_unmap(uint64_t virt, bool free_frame) {
    uint64_t *pte = walk(virt, false, false);
    if (!pte || !(*pte & PTE_P)) return;
    if (free_frame) pmm_free(*pte & PTE_ADDR);
    *pte = 0;
    invlpg(virt);
}

uint64_t vmm_translate(uint64_t virt) {
    uint64_t *t = pml4;
    uint64_t e = t[IDX4(virt)];
    if (!(e & PTE_P)) return 0;
    t = (uint64_t *)(e & PTE_ADDR);  e = t[IDX3(virt)];
    if (!(e & PTE_P)) return 0;
    t = (uint64_t *)(e & PTE_ADDR);  e = t[IDX2(virt)];
    if (!(e & PTE_P)) return 0;
    if (e & PTE_PS) return (e & PTE_ADDR & ~(LARGE_PAGE_SIZE - 1)) | (virt & (LARGE_PAGE_SIZE - 1));
    t = (uint64_t *)(e & PTE_ADDR);  e = t[IDX1(virt)];
    if (!(e & PTE_P)) return 0;
    return (e & PTE_ADDR) | (virt & (PAGE_SIZE - 1));
}

uint64_t vmm_kernel_pml4(void) { return (uint64_t)pml4; }

static void page_fault(struct regs *r) {
    uint64_t addr = read_cr2();
    kprintf("\n*** PAGE FAULT at %#llx  rip=%#llx  err=%#llx (%s, %s%s) ***\n",
            (unsigned long long)addr, (unsigned long long)r->rip, (unsigned long long)r->err_code,
            (r->err_code & 1) ? "protection violation" : "page not present",
            (r->err_code & 2) ? "write" : "read", (r->err_code & 16) ? "/fetch" : "");
    panic("unrecoverable page fault");
}

void vmm_init(void) {
    pml4 = new_table();
    if (!pml4) panic("VMM: cannot allocate PML4");

    /* 1. Kernel image area (rounded up to a 2 MiB boundary) with 4K pages and W^X. */
    uint64_t ident4k_end = ALIGN_UP((uint64_t)_kernel_end, LARGE_PAGE_SIZE);
    for (uint64_t a = PAGE_SIZE; a < ident4k_end; a += PAGE_SIZE) {
        uint32_t f = VMM_WRITE | VMM_NOEXEC;
        if (a >= (uint64_t)_text_start && a < (uint64_t)_text_end)        f = 0;                  /* R-X */
        else if (a >= (uint64_t)_rodata_start && a < (uint64_t)_rodata_end) f = VMM_NOEXEC;       /* R-- */
        else if (a >= 0xA0000 && a < 0x100000)                              f |= VMM_NOCACHE;    /* VGA etc. */
        if (vmm_map(a, a, f) < 0) panic("VMM: out of memory mapping kernel");
    }

    /* 2. Everything else: identity map all RAM with 2 MiB pages. */
    uint64_t top = ALIGN_UP(pmm_phys_top(), LARGE_PAGE_SIZE);
    if (top > PMM_MAX_PHYS) top = PMM_MAX_PHYS;
    for (uint64_t a = ident4k_end; a < top; a += LARGE_PAGE_SIZE) {
        uint64_t *t3 = next_level(pml4, IDX4(a), true, false);
        uint64_t *t2 = t3 ? next_level(t3, IDX3(a), true, false) : NULL;
        if (!t2) panic("VMM: out of memory building identity map");
        t2[IDX2(a)] = a | PTE_P | PTE_W | PTE_PS | PTE_NX;
    }

    wrmsr(0xC0000080, rdmsr(0xC0000080) | (1ULL << 11));      /* EFER.NXE */
    write_cr0(read_cr0() | (1ULL << 16));                     /* CR0.WP: kernel honours read-only pages */
    write_cr3((uint64_t)pml4);

    idt_set_handler(14, page_fault);
}
