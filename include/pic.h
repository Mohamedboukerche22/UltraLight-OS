#ifndef UL_PIC_H
#define UL_PIC_H
#include "types.h"
void pic_init(void);
void pic_unmask(uint8_t irq);
void pic_mask(uint8_t irq);
void pic_eoi(uint8_t irq);
bool pic_is_spurious(uint8_t irq);
#endif
