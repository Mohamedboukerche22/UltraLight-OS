#ifndef UL_TYPES_H
#define UL_TYPES_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#define PAGE_SIZE        4096ULL
#define LARGE_PAGE_SIZE  (2ULL * 1024 * 1024)
#define ALIGN_UP(x, a)   ((((uint64_t)(x)) + ((uint64_t)(a) - 1)) & ~((uint64_t)(a) - 1))
#define ALIGN_DOWN(x, a) (((uint64_t)(x)) & ~((uint64_t)(a) - 1))
#define PACKED           __attribute__((packed))
#define NORETURN         __attribute__((noreturn))
#define ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))

#endif
