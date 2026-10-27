/* comm/registry.c - accessors for the linker-collected command table */
#include "comm.h"
#include "string.h"

extern const command_t __comm_start[];
extern const command_t __comm_end[];

const command_t *comm_begin(void) { return __comm_start; }
const command_t *comm_end(void)   { return __comm_end; }

size_t comm_count(void) {
    return (size_t)(__comm_end - __comm_start);
}

const command_t *comm_find(const char *name) {
    for (const command_t *c = __comm_start; c < __comm_end; c++)
        if (!strcmp(c->name, name)) return c;
    return NULL;
}
