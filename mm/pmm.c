/* mm/pmm.c - bitmap physical frame allocator (1 bit per 4K frame, 1 = used) */
#include "pmm.h"
#include "string.h"
#include "panic.h"
#include "kprintf.h"

extern char _kernel_start[], _kernel_end[];

static uint64_t *bitmap;
static uint64_t  nframes, nwords, free_frames, total_frames, top_phys, hint_word;

static inline bool     fr_used(uint64_t f) { return bitmap[f >> 6] & (1ULL << (f & 63)); }
static inline void     fr_set(uint64_t f)  { bitmap[f >> 6] |=  (1ULL << (f & 63)); }
static inline void     fr_clr(uint64_t f)  { bitmap[f >> 6] &= ~(1ULL << (f & 63)); }

static void mark_used(uint64_t base, uint64_t len) {
    uint64_t first = ALIGN_DOWN(base, PAGE_SIZE) / PAGE_SIZE;
    uint64_t last  = ALIGN_UP(base + len, PAGE_SIZE) / PAGE_SIZE;
    for (uint64_t f = first; f < last && f < nframes; f++)
        if (!fr_used(f)) { fr_set(f); free_frames--; }
}

static void mark_free(uint64_t base, uint64_t len) {         /* only whole frames inside the range */
    uint64_t first = ALIGN_UP(base, PAGE_SIZE) / PAGE_SIZE;
    uint64_t last  = ALIGN_DOWN(base + len, PAGE_SIZE) / PAGE_SIZE;
    for (uint64_t f = first; f < last && f < nframes; f++)
        if (fr_used(f)) { fr_clr(f); free_frames++; }
}

void pmm_init(const bootinfo_t *bi) {
    uint64_t top = 0;
    for (int i = 0; i < bi->region_count; i++) {
        const mem_region_t *r = &bi->regions[i];
        if (r->type == MEM_AVAILABLE && r->base + r->length > top) top = r->base + r->length;
    }
    if (top > PMM_MAX_PHYS) top = PMM_MAX_PHYS;
    top_phys = ALIGN_DOWN(top, PAGE_SIZE);
    nframes  = top_phys / PAGE_SIZE;
    nwords   = (nframes + 63) / 64;
    uint64_t bytes = ALIGN_UP(nwords * 8, PAGE_SIZE);

    /* Find room for the bitmap: first usable region above the kernel (and outside bootloader data). */
    uint64_t kend = ALIGN_UP((uint64_t)_kernel_end, PAGE_SIZE), place = 0;
    for (int i = 0; i < bi->region_count && !place; i++) {
        const mem_region_t *r = &bi->regions[i];
        if (r->type != MEM_AVAILABLE) continue;
        uint64_t s = r->base > kend ? ALIGN_UP(r->base, PAGE_SIZE) : kend;
        uint64_t e = r->base + r->length;
        if (s < bi->info_end && s + bytes > bi->info_start) s = ALIGN_UP(bi->info_end, PAGE_SIZE);
        if (s + bytes <= e && s + bytes <= top_phys) place = s;
    }
    if (!place) panic("PMM: no room for frame bitmap");

    bitmap = (uint64_t *)place;
    memset(bitmap, 0xFF, nwords * 8);                         /* everything used ... */
    free_frames = 0;
    for (int i = 0; i < bi->region_count; i++)                /* ... then free what the firmware says is RAM */
        if (bi->regions[i].type == MEM_AVAILABLE) mark_free(bi->regions[i].base, bi->regions[i].length);
    for (int i = 0; i < bi->region_count; i++)                /* overlapping reserved ranges win */
        if (bi->regions[i].type != MEM_AVAILABLE) mark_used(bi->regions[i].base, bi->regions[i].length);

    mark_used(0, 0x100000);                                   /* low 1 MiB: BIOS, VGA, boot structures */
    mark_used((uint64_t)_kernel_start, (uint64_t)_kernel_end - (uint64_t)_kernel_start);
    mark_used(place, bytes);                                  /* the bitmap itself */
    if (bi->info_end > bi->info_start) mark_used(bi->info_start, bi->info_end - bi->info_start);

    total_frames = free_frames;
    hint_word = 0;
}

uint64_t pmm_alloc(void) {
    for (uint64_t n = 0; n < nwords; n++) {
        uint64_t w = (hint_word + n) % nwords;
        if (bitmap[w] != ~0ULL) {
            uint64_t bit = (uint64_t)__builtin_ctzll(~bitmap[w]);
            uint64_t f = w * 64 + bit;
            if (f >= nframes) continue;
            fr_set(f);
            free_frames--;
            hint_word = w;
            return f * PAGE_SIZE;
        }
    }
    return 0;
}

uint64_t pmm_alloc_contig(size_t n) {
    if (n == 0) return 0;
    uint64_t run = 0;
    for (uint64_t f = 0; f < nframes; f++) {
        run = fr_used(f) ? 0 : run + 1;
        if (run == n) {
            uint64_t start = f + 1 - n;
            for (uint64_t i = start; i <= f; i++) fr_set(i);
            free_frames -= n;
            return start * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free(uint64_t phys) {
    uint64_t f = phys / PAGE_SIZE;
    if ((phys & (PAGE_SIZE - 1)) || f >= nframes) panic("pmm_free: bad address %#llx", (unsigned long long)phys);
    if (!fr_used(f)) panic("pmm_free: double free of %#llx", (unsigned long long)phys);
    fr_clr(f);
    free_frames++;
    if (f / 64 < hint_word) hint_word = f / 64;
}

void pmm_free_contig(uint64_t phys, size_t n) { for (size_t i = 0; i < n; i++) pmm_free(phys + i * PAGE_SIZE); }

uint64_t pmm_total_frames(void) { return total_frames; }
uint64_t pmm_free_frames(void)  { return free_frames; }
uint64_t pmm_phys_top(void)     { return top_phys; }
