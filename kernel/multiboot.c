/* kernel/multiboot.c - normalise Multiboot1 / Multiboot2 boot info into bootinfo_t */
#include "multiboot.h"
#include "string.h"
#include "panic.h"

struct mb1_info {
    uint32_t flags, mem_lower, mem_upper, boot_device, cmdline, mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length, mmap_addr;
    uint32_t drives_length, drives_addr, config_table, boot_loader_name;
} PACKED;

struct mb1_mmap { uint32_t size; uint64_t base, length; uint32_t type; } PACKED;

struct mb2_tag { uint32_t type, size; } PACKED;
struct mb2_mmap_tag { uint32_t type, size, entry_size, entry_version; } PACKED;
struct mb2_mmap_entry { uint64_t base, length; uint32_t type, reserved; } PACKED;

static void copy_cstr(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    for (; src && src[i] && i + 1 < cap; i++) dst[i] = src[i];
    dst[i] = 0;
}

static void add_region(bootinfo_t *bi, uint64_t base, uint64_t len, uint32_t type) {
    if (bi->region_count >= MAX_REGIONS || len == 0) return;
    bi->regions[bi->region_count++] = (mem_region_t){ base, len, type };
}

static void extend_info(bootinfo_t *bi, uint64_t start, uint64_t len) {
    if (start < bi->info_start) bi->info_start = start;
    if (start + len > bi->info_end) bi->info_end = start + len;
}

void multiboot_parse(uint32_t magic, uint64_t addr, bootinfo_t *bi) {
    memset(bi, 0, sizeof *bi);
    bi->info_start = ~0ULL;

    if (magic == MB1_BOOT_MAGIC) {
        const struct mb1_info *mb = (const struct mb1_info *)addr;
        bi->protocol = 1;
        extend_info(bi, addr, sizeof *mb);
        if (mb->flags & (1u << 2)) copy_cstr(bi->cmdline, sizeof bi->cmdline, (const char *)(uintptr_t)mb->cmdline);
        if (mb->flags & (1u << 9)) copy_cstr(bi->loader, sizeof bi->loader, (const char *)(uintptr_t)mb->boot_loader_name);
        if (mb->flags & (1u << 6)) {
            extend_info(bi, mb->mmap_addr, mb->mmap_length);
            uint64_t p = mb->mmap_addr, end = p + mb->mmap_length;
            while (p < end) {
                const struct mb1_mmap *e = (const struct mb1_mmap *)p;
                add_region(bi, e->base, e->length, e->type);
                p += e->size + 4;
            }
        } else if (mb->flags & (1u << 0)) {
            add_region(bi, 0x100000, (uint64_t)mb->mem_upper * 1024, MEM_AVAILABLE);
        }
    } else if (magic == MB2_BOOT_MAGIC) {
        uint32_t total = *(const uint32_t *)addr;
        bi->protocol = 2;
        extend_info(bi, addr, total);
        const uint8_t *p = (const uint8_t *)addr + 8, *end = (const uint8_t *)addr + total;
        while (p + sizeof(struct mb2_tag) <= end) {
            const struct mb2_tag *t = (const struct mb2_tag *)p;
            if (t->type == 0) break;
            if (t->type == 1) copy_cstr(bi->cmdline, sizeof bi->cmdline, (const char *)(t + 1));
            else if (t->type == 2) copy_cstr(bi->loader, sizeof bi->loader, (const char *)(t + 1));
            else if (t->type == 6) {
                const struct mb2_mmap_tag *m = (const struct mb2_mmap_tag *)t;
                for (const uint8_t *e = p + sizeof *m; e + m->entry_size <= p + t->size; e += m->entry_size) {
                    const struct mb2_mmap_entry *me = (const struct mb2_mmap_entry *)e;
                    add_region(bi, me->base, me->length, me->type);
                }
            }
            p += ALIGN_UP(t->size, 8);
        }
    } else {
        panic("not booted by a Multiboot loader (magic=%#x)", magic);
    }

    if (bi->region_count == 0) panic("bootloader gave no memory map");
}
