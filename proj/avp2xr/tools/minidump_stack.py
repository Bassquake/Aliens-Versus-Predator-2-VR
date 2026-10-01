import struct, sys
path = sys.argv[1]
d = open(path, 'rb').read()
sig, ver, nstreams, dirrva = struct.unpack_from('<IIII', d, 0)
streams = {}
for i in range(nstreams):
    t, size, rva = struct.unpack_from('<III', d, dirrva + i * 12)
    streams.setdefault(t, []).append((size, rva))


def rstr(rva):
    n = struct.unpack_from('<I', d, rva)[0]
    return d[rva + 4:rva + 4 + n].decode('utf-16le', 'replace')


# modules
mods = []
size, rva = streams[4][0]
n = struct.unpack_from('<I', d, rva)[0]
for i in range(n):
    m = rva + 4 + i * 108
    base, msize, csum, ts, namerva = struct.unpack_from('<QIIII', d, m)
    mods.append((base, msize, rstr(namerva).split('\\')[-1], ts))


def modof(a):
    for base, msize, name, ts in mods:
        if base <= a < base + msize:
            return name, a - base
    return None, None


# memory (Memory64List or MemoryList)
ranges = []
if 9 in streams:
    size, rva = streams[9][0]
    nr, baserva = struct.unpack_from('<QQ', d, rva)
    off = baserva
    for i in range(nr):
        start, sz = struct.unpack_from('<QQ', d, rva + 16 + i * 16)
        ranges.append((start, sz, off))
        off += sz
if 5 in streams:
    size, rva = streams[5][0]
    nr = struct.unpack_from('<I', d, rva)[0]
    for i in range(nr):
        start, sz, mrva = struct.unpack_from('<QII', d, rva + 4 + i * 16)
        ranges.append((start, sz, mrva))


def read(a, n):
    for start, sz, off in ranges:
        if start <= a and a + n <= start + sz:
            return d[off + (a - start):off + (a - start) + n]
    return None


# exception
size, rva = streams[6][0]
tid = struct.unpack_from('<I', d, rva)[0]
code, flags = struct.unpack_from('<II', d, rva + 8)
addr = struct.unpack_from('<Q', d, rva + 8 + 16)[0]
ctxsize, ctxrva = struct.unpack_from('<II', d, rva + 8 + 152)
print('exception thread %d code %08x at %08x %s' % (tid, code, addr, modof(addr)))
for base, msize, name, ts in mods:
    if 'cshell' in name.lower() or 'lithtech' in name.lower() or 'd3d11' in name.lower():
        print('module %-20s base %08x size %x ts %x' % (name, base, msize, ts))
# x86 CONTEXT: Esp at 0xC4, Ebp 0xB4, Eip 0xB8
ctx = d[ctxrva:ctxrva + ctxsize]
ebp, eip = struct.unpack_from('<II', ctx, 0xB4)
esp = struct.unpack_from('<I', ctx, 0xC4)[0]
print('eip %08x %s esp %08x ebp %08x' % (eip, modof(eip), esp, ebp))
print('ranges', len(ranges))
for i in range(0, 0x3000, 4):
        w = read(esp + i, 4)
        if w is None:
            continue
        v = struct.unpack_from('<I', w, 0)[0]
        name, off = modof(v)
        if name and name.lower() in ('cshell.dll', 'lithtech.exe', 'd3d.ren', 'object.lto'):
            print('  [esp+%04x] %08x %s+%x' % (i, v, name, off))
