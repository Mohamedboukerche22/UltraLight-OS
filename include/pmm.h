#ifndef UL_PMM_H
#define UL_PMM_H
#include "types.h"
#include "multiboot.h"
#define PMM_MAX_PHYS (4ULL * 1024 * 1024 * 1024)   /* everything is identity mapped, so cap at 4 GiB */
void     pmm_init(const bootinfo_t *bi);
uint64_t pmm_alloc(void);                 /* one 4K frame, 0 on failure */
uint64_t pmm_alloc_contig(size_t n);      /* n contiguous frames, 0 on failure */
void     pmm_free(uint64_t phys);
void     pmm_free_contig(uint64_t phys, size_t n);
uint64_t pmm_total_frames(void);
uint64_t pmm_free_frames(void);
uint64_t pmm_phys_top(void);
#endif
