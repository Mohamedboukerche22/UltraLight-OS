#ifndef UL_KPRINTF_H
#define UL_KPRINTF_H
#include "types.h"
/* Formats: %c %s %d %i %u %x %X %p %% with l/ll/z length modifiers, width, '0' and '-' flags. */
void kvprintf_cb(void (*out)(char), const char *fmt, va_list ap);
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));   /* VGA + serial */
void klog(const char *fmt, ...)    __attribute__((format(printf, 1, 2)));   /* serial only  */
#endif
