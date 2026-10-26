#ifndef UL_VMM_H
#define UL_VMM_H
#include "types.h"
#define VMM_WRITE   (1u << 0)
#define VMM_USER    (1u << 1)
#define VMM_NOEXEC  (1u << 2)
#define VMM_NOCACHE (1u << 3)

void     vmm_init(void);
int      vmm_map(uint64_t virt, uint64_t phys, uint32_t flags);       /* 0 ok, -1 fail */
int      vmm_alloc(uint64_t virt, size_t npages, uint32_t flags);     /* map fresh zeroed frames */
void     vmm_unmap(uint64_t virt, bool free_frame);
uint64_t vmm_translate(uint64_t virt);                                /* 0 if unmapped */
uint64_t vmm_kernel_pml4(void);
#endif
