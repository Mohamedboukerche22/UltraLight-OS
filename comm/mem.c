/* comm/mem.c - physical memory + heap statistics */
#include "comm.h"
#include "kprintf.h"
#include "heap.h"
#include "pmm.h"
#include "vmm.h"

static void cmd_mem(int argc, char **argv) {
    (void)argc; (void)argv;
    heap_stats_t h; heap_stats(&h);
    unsigned long long tot = pmm_total_frames(), fr = pmm_free_frames();
    kprintf("Physical: %llu KiB total, %llu KiB used, %llu KiB free (%llu frames free)\n",
            tot * 4, (tot - fr) * 4, fr * 4, fr);
    kprintf("Heap:     %llu KiB mapped, %llu B used, %llu B free, %llu blocks\n",
            (unsigned long long)h.heap_size / 1024, (unsigned long long)h.used,
            (unsigned long long)h.free, (unsigned long long)h.blocks);
    kprintf("PML4 at   %#llx\n", (unsigned long long)vmm_kernel_pml4());
}
COMMAND("mem", "physical memory + heap statistics", cmd_mem);
