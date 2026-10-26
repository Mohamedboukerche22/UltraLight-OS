#ifndef UL_PANIC_H
#define UL_PANIC_H
#include "types.h"
void panic(const char *fmt, ...) NORETURN __attribute__((format(printf, 1, 2)));
#endif
