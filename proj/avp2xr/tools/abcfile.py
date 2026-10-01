import struct


class Abc:
    """LithTech ABC v12 model: the sections as raw bytes, plus the header's command string."""

    def __init__(self, path):
        d = open(path, 'rb').read()
        self.sections = []
        p = 0
        while p < len(d):
            n = struct.unpack_from('<H', d, p)[0]
            name = d[p + 2:p + 2 + n].decode('latin1')
            nxt = struct.unpack_from('<i', d, p + 2 + n)[0]
            start = p + 2 + n + 4
            end = nxt if nxt > 0 else len(d)
            self.sections.append([name, d[start:end]])
            if nxt <= 0:
                break
            p = nxt
        assert self.sections[0][0] == 'Header'
        h = self.sections[0][1]
        self.head_ints = list(struct.unpack_from('<14I', h, 0))
        clen = struct.unpack_from('<H', h, 56)[0]
        self.command = h[58:58 + clen].decode('latin1')
        self.head_rest = h[58 + clen:]

    # header fields (guessed from the files): version, keyframes, anims, nodes, pieces, child models,
    # tris, verts, vertex weights, LODs, sockets, weight sets, strings, total string length
    def build(self):
        h = self.sections[0]
        cmd = self.command.encode('latin1')
        h[1] = struct.pack('<14I', *self.head_ints) + struct.pack('<H', len(cmd)) + cmd + self.head_rest
        out = bytearray()
        for i, (name, data) in enumerate(self.sections):
            nb = name.encode('latin1')
            start = len(out)
            last = i == len(self.sections) - 1
            nxt = -1 if last else start + 2 + len(nb) + 4 + len(data)
            out += struct.pack('<H', len(nb)) + nb + struct.pack('<i', nxt) + data
        return bytes(out)


def count_strings(path):
    """Every length-prefixed string in the model, to check the header's string counts."""
    a = Abc(path)
    sec = dict(a.sections)
    strs = [a.command]
    d = sec['Nodes']
    p = 0

    def rd(p):
        n = struct.unpack_from('<H', d, p)[0]
        strs.append(d[p + 2:p + 2 + n].decode('latin1'))
        p += 2 + n + 3 + 64
        nc = struct.unpack_from('<I', d, p)[0]
        p += 4
        for _ in range(nc):
            p = rd(p)
        return p
    rd(0)
    nnodes = a.head_ints[3]
    # pieces: names
    d = sec['Pieces']
    p = 8
    for _ in range(a.head_ints[4]):
        p += 2 + 8 + 4 + 2
        n = struct.unpack_from('<H', d, p)[0]
        strs.append(d[p + 2:p + 2 + n].decode('latin1'))
        p += 2 + n
        nt = struct.unpack_from('<I', d, p)[0]
        p += 4 + nt * 30
        nv = struct.unpack_from('<I', d, p)[0]
        p += 4
        for _ in range(nv):
            nw = struct.unpack_from('<H', d, p)[0]
            p += 4 + nw * 20 + 24
    # animations
    d = sec['Animation']
    p = 0
    nanims = struct.unpack_from('<I', d, p)[0]
    p += 4
    for _ in range(nanims):
        p += 12
        n = struct.unpack_from('<H', d, p)[0]
        strs.append(d[p + 2:p + 2 + n].decode('latin1'))
        p += 2 + n
        p += 8   # compression?, interpolation ms
        nk = struct.unpack_from('<I', d, p)[0]
        p += 4
        for _ in range(nk):
            p += 4
            n = struct.unpack_from('<H', d, p)[0]
            s = d[p + 2:p + 2 + n].decode('latin1')
            if s:
                strs.append(s)
            p += 2 + n
        p += nnodes * nk * 28
    anim_end_ok = p == len(d)
    # sockets
    d = sec['Sockets']
    ns = struct.unpack_from('<I', d, 0)[0]
    p = 4
    for _ in range(ns):
        p += 4
        n = struct.unpack_from('<H', d, p)[0]
        strs.append(d[p + 2:p + 2 + n].decode('latin1'))
        p += 2 + n + 28
    return a, strs, anim_end_ok
