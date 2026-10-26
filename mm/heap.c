/* mm/heap.c - kernel heap: first-fit free list with splitting + coalescing.
 *
 * The heap lives in its own virtual region (outside the identity map) and grows
 * on demand by asking the VMM to map fresh frames.  Every block is physically
 * adjacent to its list neighbour, so coalescing is a simple pointer check.
 */
#include "heap.h"
#include "vmm.h"
#include "string.h"
#include "panic.h"

#ifndef HEAP_BASE
#define HEAP_BASE 0x0000008000000000ULL          /* 512 GiB: PML4 slot 1, away from the identity map */
#endif
#define HEAP_LIMIT   (1ULL << 30)                /* 1 GiB max */
#define HEAP_GROW_MIN (16 * PAGE_SIZE)
#define BLK_MAGIC    0xB10CC0DEu

typedef struct block {
    size_t        size;                          /* payload bytes */
    uint32_t      magic;
    uint32_t      free;
    struct block *next, *prev;
} block_t;                                       /* 32 bytes: keeps payloads 16-aligned */

#define HDR sizeof(block_t)
#define MIN_SPLIT (HDR + 16)

static block_t *head, *tail;
static uint64_t heap_start, heap_end;

static bool grow(size_t need) {
    size_t bytes = ALIGN_UP(need + HDR, PAGE_SIZE);
    if (bytes < HEAP_GROW_MIN) bytes = HEAP_GROW_MIN;
    if (heap_end + bytes > heap_start + HEAP_LIMIT) return false;
    if (vmm_alloc(heap_end, bytes / PAGE_SIZE, VMM_WRITE | VMM_NOEXEC) < 0) return false;

    block_t *b = (block_t *)heap_end;
    b->size = bytes - HDR; b->magic = BLK_MAGIC; b->free = 1; b->next = NULL; b->prev = tail;
    if (tail) tail->next = b; else head = b;
    tail = b;
    heap_end += bytes;

    if (b->prev && b->prev->free) {              /* merge with a free tail */
        b->prev->size += HDR + b->size;
        b->prev->next = NULL;
        tail = b->prev;
    }
    return true;
}

void heap_init(void) {
    heap_start = heap_end = HEAP_BASE;
    head = tail = NULL;
    if (!grow(0)) panic("heap: cannot create initial heap");
}

static void split(block_t *b, size_t n) {
    if (b->size >= n + MIN_SPLIT) {
        block_t *r = (block_t *)((uint8_t *)b + HDR + n);
        r->size = b->size - n - HDR; r->magic = BLK_MAGIC; r->free = 1;
        r->next = b->next; r->prev = b;
        if (b->next) b->next->prev = r; else tail = r;
        b->next = r;
        b->size = n;
    }
}

void *kmalloc(size_t n) {
    if (n == 0) return NULL;
    n = ALIGN_UP(n, 16);
    for (;;) {
        for (block_t *b = head; b; b = b->next) {
            if (b->free && b->size >= n) {
                split(b, n);
                b->free = 0;
                return (uint8_t *)b + HDR;
            }
        }
        if (!grow(n)) return NULL;
    }
}

void *kcalloc(size_t n, size_t sz) {
    if (sz && n > (size_t)-1 / sz) return NULL;
    void *p = kmalloc(n * sz);
    if (p) memset(p, 0, n * sz);
    return p;
}

void kfree(void *p) {
    if (!p) return;
    if ((uint64_t)p < heap_start + HDR || (uint64_t)p >= heap_end) panic("kfree: %p is not a heap pointer", p);
    block_t *b = (block_t *)((uint8_t *)p - HDR);
    if (b->magic != BLK_MAGIC) panic("kfree: corrupted block header at %p", p);
    if (b->free) panic("kfree: double free of %p", p);
    b->free = 1;

    if (b->next && b->next->free) {              /* merge forward */
        block_t *n = b->next;
        b->size += HDR + n->size;
        b->next = n->next;
        if (n->next) n->next->prev = b; else tail = b;
        n->magic = 0;
    }
    if (b->prev && b->prev->free) {              /* merge backward */
        block_t *pr = b->prev;
        pr->size += HDR + b->size;
        pr->next = b->next;
        if (b->next) b->next->prev = pr; else tail = pr;
        b->magic = 0;
    }
}

void *krealloc(void *p, size_t n) {
    if (!p) return kmalloc(n);
    if (n == 0) { kfree(p); return NULL; }
    block_t *b = (block_t *)((uint8_t *)p - HDR);
    if (b->magic != BLK_MAGIC || b->free) panic("krealloc: bad pointer %p", p);
    if (b->size >= n) return p;
    void *q = kmalloc(n);
    if (!q) return NULL;
    memcpy(q, p, b->size);
    kfree(p);
    return q;
}

void heap_stats(heap_stats_t *s) {
    memset(s, 0, sizeof *s);
    for (block_t *b = head; b; b = b->next) {
        s->blocks++;
        if (b->free) s->free += b->size; else s->used += b->size;
    }
    s->heap_size = heap_end - heap_start;
}
