/* comm/memtest.c - stress-test PMM, VMM and kmalloc/kfree */
#include "comm.h"
#include "kprintf.h"
#include "string.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"

static void cmd_memtest(int argc, char **argv) {
    (void)argc; (void)argv;
    enum { N = 64 };
    uint8_t *p[N];
    heap_stats_t before, after; heap_stats(&before);
    for (int i = 0; i < N; i++) {
        size_t sz = 16 + (size_t)(i * 37) % 3000;
        p[i] = kmalloc(sz);
        if (!p[i]) { kprintf("FAIL: kmalloc(%zu) returned NULL\n", sz); return; }
        memset(p[i], i + 1, sz);
        p[i][0] = (uint8_t)(i + 1);
    }
    for (int i = 0; i < N; i++) {
        size_t sz = 16 + (size_t)(i * 37) % 3000;
        for (size_t j = 0; j < sz; j++)
            if (p[i][j] != (uint8_t)(i + 1)) { kprintf("FAIL: corruption in block %d at +%zu\n", i, j); return; }
    }
    for (int i = 0; i < N; i += 2) kfree(p[i]);
    for (int i = 1; i < N; i += 2) kfree(p[i]);
    void *big = kmalloc(200 * 1024);                       /* forces heap growth */
    if (!big) { kprintf("FAIL: big allocation\n"); return; }
    memset(big, 0xAB, 200 * 1024);
    kfree(big);
    heap_stats(&after);
    kprintf("kmalloc/kfree: %d blocks verified; heap %llu -> %llu KiB; used bytes after: %llu\n", N,
            (unsigned long long)before.heap_size / 1024, (unsigned long long)after.heap_size / 1024,
            (unsigned long long)after.used);

    uint64_t a = pmm_alloc(), b = pmm_alloc();
    kprintf("pmm_alloc: %#llx, %#llx  ", (unsigned long long)a, (unsigned long long)b);
    pmm_free(a); pmm_free(b);
    kprintf("(freed)\nvmm_translate(heap base) = %#llx\n",
            (unsigned long long)vmm_translate(0x0000008000000000ULL));
    kprintf("memtest PASSED\n");
}
COMMAND("memtest", "stress-test PMM, VMM and kmalloc", cmd_memtest);
