#!/usr/bin/env python3
"""tools/mkiso.py - tiny dependency-free ISO9660 + El Torito BIOS image builder.

Usage:
    mkiso.py <root-directory> <output.iso> <boot-image-relative-path>

The output is a plain ISO9660 image with one El Torito no-emulation boot
entry.  It is enough for Limine's limine-bios-cd.bin and works with QEMU,
real CD/DVD drives and BIOS-only machines.  No xorriso/grub/mtools needed.

This is intentionally minimal: it supports the files and directories needed
by this project (nested directories, normal ISO9660 names, no Rock Ridge/Joliet).
"""

import os
import struct
import sys
import time

BLOCK = 2048


def both16(v):
    return struct.pack('<H', v) + struct.pack('>H', v)


def both32(v):
    return struct.pack('<I', v) + struct.pack('>I', v)


def ceil_div(a, b):
    return (a + b - 1) // b


def iso_name(name):
    """Convert a host filename to a safe ISO9660 Level 1 identifier."""
    out = []
    for ch in name.upper():
        if ('A' <= ch <= 'Z') or ('0' <= ch <= '9') or ch in '_.-':
            out.append(ch)
        else:
            out.append('_')
    s = ''.join(out)
    if not s:
        s = '_'
    return s


class Node:
    def __init__(self, name, is_dir=False):
        self.name = name
        self.is_dir = is_dir
        self.children = []
        self.data = b''
        self.lba = 0
        self.size = 0
        self.parent = None
        self.number = 0
        self.path = ''

    @property
    def identifier(self):
        if self.is_dir:
            return iso_name(self.name)[:31].encode('ascii')
        # ISO9660 Level 2 allows up to 30 chars before the version suffix.
        # Keeping the full name matters: Limine looks for "limine-bios.sys",
        # "limine.conf" and the kernel path from limine.conf.
        base = iso_name(self.name)[:30]
        return (base + ';1').encode('ascii')


def build_tree(root):
    root = os.path.abspath(root)
    root_node = Node('', is_dir=True)
    root_node.path = ''
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        filenames.sort()
        rel = os.path.relpath(dirpath, root)
        if rel == '.':
            parent = root_node
        else:
            parent = find_node(root_node, rel.split(os.sep))
        for d in dirnames:
            n = Node(d, is_dir=True)
            n.parent = parent
            n.path = (parent.path + '/' + d).lstrip('/')
            parent.children.append(n)
        for f in filenames:
            n = Node(f)
            n.parent = parent
            n.path = (parent.path + '/' + f).lstrip('/')
            with open(os.path.join(dirpath, f), 'rb') as fh:
                n.data = fh.read()
            n.size = len(n.data)
            parent.children.append(n)
    return root_node


def find_node(root, parts):
    cur = root
    for p in parts:
        for c in cur.children:
            if c.is_dir and c.name == p:
                cur = c
                break
        else:
            raise KeyError('/'.join(parts))
    return cur


def dirs_bfs(root):
    out = [root]
    root.number = 1
    i = 0
    while i < len(out):
        cur = out[i]
        i += 1
        for c in cur.children:
            if c.is_dir:
                c.number = len(out) + 1
                out.append(c)
    return out


def iso_date(ts=None):
    t = time.gmtime(ts)
    return bytes([t.tm_year - 1900, t.tm_mon, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec, 0])


def pvd_timestamp(ts=None):
    t = time.gmtime(ts)
    return (time.strftime('%Y%m%d%H%M%S', t) + '00').encode('ascii') + b'\x00'


def directory_record(identifier, lba, size, is_dir, ts=None):
    ident = bytes(identifier)
    length = 33 + len(ident)
    if len(ident) % 2 == 0:          # fixed part is odd, pad to even length
        length += 1
    rec = bytearray(length)
    rec[0] = length
    rec[1] = 0
    rec[2:10] = both32(lba)
    rec[10:18] = both32(size)
    rec[18:25] = iso_date(ts)
    rec[25] = 2 if is_dir else 0
    rec[26] = 0
    rec[27] = 0
    rec[28:32] = both16(1)
    rec[32] = len(ident)
    rec[33:33 + len(ident)] = ident
    return bytes(rec)


def build_dir_data(node, ts):
    """Return the complete directory data (sector-padded) for a directory."""
    recs = []
    parent = node.parent if node.parent is not None else node
    recs.append(directory_record(b'\x00', node.lba, node.size, True, ts))
    recs.append(directory_record(b'\x01', parent.lba, parent.size, True, ts))

    for child in sorted(node.children, key=lambda n: n.identifier):
        recs.append(directory_record(child.identifier, child.lba, child.size,
                                     child.is_dir, ts))

    out = bytearray()
    for rec in recs:
        if len(out) % BLOCK + len(rec) > BLOCK:
            out += b'\x00' * (BLOCK - (len(out) % BLOCK))
        out += rec
    out += b'\x00' * ((-len(out)) % BLOCK)
    return bytes(out)


def path_table(dirs, big_endian):
    end = '>' if big_endian else '<'
    out = bytearray()
    for d in dirs:
        if d.parent is None:
            ident = b'\x00'
            parent_num = d.number
        else:
            ident = d.identifier
            parent_num = d.parent.number
        rec = bytearray()
        rec += bytes([len(ident), 0])
        rec += struct.pack(end + 'I', d.lba)
        rec += struct.pack(end + 'H', parent_num)
        rec += ident
        if len(ident) % 2:
            rec += b'\x00'
        out += rec
    return bytes(out)


def build_catalog(boot_lba, boot_size):
    entry = bytearray(2048)
    # Validation entry (32 bytes)
    entry[0] = 1                    # header id
    entry[1] = 0                    # platform: x86
    entry[4:4 + 24] = b'ULTRALIGHT BIOS BOOT'.ljust(24, b'\x00')
    entry[30:32] = b'\x55\xaa'
    words = struct.unpack('<16H', bytes(entry[0:32]))
    entry[28:30] = struct.pack('<H', (-sum(words)) & 0xffff)

    # Initial/default entry (32 bytes)
    e = 32
    entry[e + 0] = 0x88             # bootable
    entry[e + 1] = 0                # no emulation
    entry[e + 2:e + 4] = struct.pack('<H', 0)       # load segment
    entry[e + 4] = 0                # system type
    entry[e + 5] = 0
    # Only the 2 KiB bootsector sector is loaded by the BIOS; the boot image
    # then reads the rest of itself using the El Torito boot info table.
    entry[e + 6:e + 8] = struct.pack('<H', 4)
    entry[e + 8:e + 12] = struct.pack('<I', boot_lba)
    return bytes(entry)


def boot_info_table(data, boot_lba):
    """Patch the El Torito Boot Information Table used by Limine/ISOLINUX.

    Layout (at offset 8):
        +0  PVD LBA
        +4  boot image LBA (in 2048-byte blocks)
        +8  boot image length
        +12 boot image checksum
        +16 40 reserved bytes
    """
    img = bytearray(data)
    if len(img) < 64:
        return bytes(img)
    struct.pack_into('<I', img, 8, 16)
    struct.pack_into('<I', img, 12, boot_lba)
    struct.pack_into('<I', img, 16, len(img))
    struct.pack_into('<I', img, 20, 0)
    total = 0
    for off in range(0, len(img) - 3, 4):
        total = (total + struct.unpack_from('<I', img, off)[0]) & 0xffffffff
    struct.pack_into('<I', img, 20, (-total) & 0xffffffff)
    return bytes(img)


def main(argv):
    if len(argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2

    root_dir, out_path, boot_rel = argv[1], argv[2], argv[3].replace('\\', '/')
    root = build_tree(root_dir)
    dirs = dirs_bfs(root)
    ts = time.time()

    # Locate the El Torito boot image.
    boot = None
    for d in dirs:
        for c in d.children:
            if not c.is_dir and c.path == boot_rel:
                boot = c
                break
        if boot:
            break
    if boot is None:
        print(f'mkiso: boot image not found in root: {boot_rel}', file=sys.stderr)
        return 1
    if boot.size == 0:
        print('mkiso: boot image is empty', file=sys.stderr)
        return 1

    # --- first pass: sizes and sector layout --------------------------------
    # Path tables are tiny, but allocate as many sectors as needed.
    probe_dirs = dirs
    for d in probe_dirs:
        d.lba = 0
        d.size = 0
    probe_pt_len = len(path_table(probe_dirs, False))
    pt_sectors = ceil_div(probe_pt_len, BLOCK)
    l_pt_lba = 19
    m_pt_lba = 19 + pt_sectors

    next_lba = 19 + 2 * pt_sectors

    # Directory sizes do not depend on LBAs, only on identifiers.
    for d in dirs:
        d.lba = next_lba
        d.size = len(build_dir_data(d, ts))
        next_lba += ceil_div(d.size, BLOCK)

    catalog_lba = next_lba
    next_lba += 1

    boot.lba = next_lba
    next_lba += ceil_div(boot.size, BLOCK)

    files = []
    for d in dirs:
        for c in d.children:
            if not c.is_dir and c is not boot:
                files.append(c)
    files.sort(key=lambda n: n.path)
    for f in files:
        f.lba = next_lba
        next_lba += ceil_div(f.size, BLOCK)

    # Leave a little tail padding.  Some CD/BIOS stacks refuse a read whose
    # transfer block ends exactly at the last reported sector of the medium.
    total_sectors = next_lba + 32

    # Rebuild path table now that LBAs are known.
    pt_l = path_table(dirs, False)
    pt_m = path_table(dirs, True)
    assert len(pt_l) == len(pt_m) == probe_pt_len

    # --- second pass: directory data and metadata ---------------------------
    dir_data = {d.number: build_dir_data(d, ts) for d in dirs}

    pvd = bytearray(BLOCK)
    pvd[0] = 1
    pvd[1:6] = b'CD001'
    pvd[6] = 1
    pvd[8:40] = b'ULTRALIGHT'.ljust(32, b' ')
    pvd[40:72] = b'ULTRALIGHT_OS'.ljust(32, b' ')
    pvd[80:88] = both32(total_sectors)
    pvd[120:124] = both16(1)
    pvd[124:128] = both16(1)
    pvd[128:132] = both16(BLOCK)
    pvd[132:140] = both32(len(pt_l))
    pvd[140:144] = struct.pack('<I', l_pt_lba)
    pvd[144:148] = struct.pack('<I', 0)
    pvd[148:152] = struct.pack('>I', m_pt_lba)
    pvd[152:156] = struct.pack('>I', 0)

    root_rec = directory_record(b'\x00', root.lba, root.size, True, ts)
    pvd[156:156 + len(root_rec)] = root_rec

    pvd[190:318] = b' '.ljust(128, b' ')
    pvd[318:446] = b' '.ljust(128, b' ')
    pvd[446:574] = b' '.ljust(128, b' ')
    pvd[574:702] = b'ULTRALIGHT MKISO'.ljust(128, b' ')
    pvd[702:739] = b' '.ljust(37, b' ')
    pvd[739:776] = b' '.ljust(37, b' ')
    pvd[776:813] = b' '.ljust(37, b' ')
    pvd[813:830] = pvd_timestamp(ts)
    pvd[830:847] = pvd_timestamp(ts)
    pvd[847:864] = pvd_timestamp(ts)
    pvd[864:881] = pvd_timestamp(ts)
    pvd[881] = 1

    brvd = bytearray(BLOCK)
    brvd[0] = 0
    brvd[1:6] = b'CD001'
    brvd[6] = 1
    brvd[7:39] = b'EL TORITO SPECIFICATION'.ljust(32, b'\x00')
    brvd[71:75] = struct.pack('<I', catalog_lba)

    term = bytearray(BLOCK)
    term[0] = 255
    term[1:6] = b'CD001'
    term[6] = 1

    catalog = build_catalog(boot.lba, boot.size)

    # --- assemble ------------------------------------------------------------
    image = bytearray(total_sectors * BLOCK)

    def put(lba, data):
        off = lba * BLOCK
        image[off:off + len(data)] = data

    put(16, bytes(pvd))
    put(17, bytes(brvd))
    put(18, bytes(term))
    put(l_pt_lba, pt_l)
    put(m_pt_lba, pt_m)
    for d in dirs:
        put(d.lba, dir_data[d.number])
    put(catalog_lba, catalog)
    put(boot.lba, boot_info_table(boot.data, boot.lba))
    for f in files:
        put(f.lba, f.data)

    with open(out_path, 'wb') as fh:
        fh.write(image)

    print(f'mkiso: wrote {out_path} ({len(image)} bytes, {total_sectors} sectors)')
    print(f'mkiso: boot image {boot_rel} at LBA {boot.lba} '
          f'({ceil_div(boot.size, BLOCK)} x 2048-byte blocks)')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
