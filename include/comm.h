/* include/comm.h - command registry for the UltraLight shell
 *
 * Every shell command lives in its own file under comm/.  A command
 * registers itself with the COMMAND() macro; the linker collects all of
 * them into the .comm_commands section.  Adding a command later means
 * dropping a new .c file into comm/ - the Makefile wildcard compiles it
 * and no kernel/shell/registry file has to be touched.
 */
#ifndef UL_COMM_H
#define UL_COMM_H
#include "types.h"

typedef struct command {
    const char *name;
    const char *help;
    void (*fn)(int argc, char **argv);
} __attribute__((aligned(16))) command_t;
/* The 16-byte alignment makes sizeof(command_t) a multiple of 16, so GCC
 * cannot insert padding between entries in .comm_commands and comm_count()/
 * pointer iteration stay exact. */

#define COMMAND(cmd_name, cmd_help, cmd_fn)                                   \
    static const command_t command_entry_##cmd_fn                             \
        __attribute__((used, section(".comm_commands"))) = {                  \
            (cmd_name), (cmd_help), (cmd_fn)                                  \
        }

/* Implemented in comm/registry.c; bounds come from linker.ld. */
const command_t *comm_begin(void);
const command_t *comm_end(void);
size_t           comm_count(void);
const command_t *comm_find(const char *name);

#endif
