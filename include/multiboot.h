#ifndef UL_MULTIBOOT_H
#define UL_MULTIBOOT_H
#include "types.h"

#define MB1_BOOT_MAGIC 0x2BADB002u
#define MB2_BOOT_MAGIC 0x36D76289u
#define MEM_AVAILABLE  1u
#define MAX_REGIONS    64

typedef struct { uint64_t base, length; uint32_t type; } mem_region_t;

typedef struct {
    int          protocol;                 /* 1 or 2 */
    int          region_count;
    mem_region_t regions[MAX_REGIONS];
    uint64_t     info_start, info_end;     /* bootloader data we must not overwrite */
    char         cmdline[128];
    char         loader[64];
} bootinfo_t;

void multiboot_parse(uint32_t magic, uint64_t info_addr, bootinfo_t *out);
const bootinfo_t *multiboot_bootinfo(void);   /* last parsed boot info, or NULL */
#endif
