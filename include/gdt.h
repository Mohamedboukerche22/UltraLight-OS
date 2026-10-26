#ifndef UL_GDT_H
#define UL_GDT_H
#include "types.h"
#define GDT_KCODE 0x08
#define GDT_KDATA 0x10
#define GDT_UDATA 0x1B
#define GDT_UCODE 0x23
void gdt_init(void);
void tss_set_rsp0(uint64_t rsp0);
#endif
