import struct, glob, os, re
base = "C:/Program Files (x86)/Fox/Aliens vs. Predator 2/AVP2/Models/Weapons/"


def load(path):
    d = open(path, 'rb').read()
    secs = {}
    p = 0
    while p < len(d):
        n = struct.unpack_from('<H', d, p)[0]
        name = d[p + 2:p + 2 + n].decode('latin1')
        nxt = struct.unpack_from('<i', d, p + 2 + n)[0]
        secs[name] = p + 2 + n + 4
        if nxt <= p:
            break
        p = nxt
    nodes, parent = [], []
    q = secs['Nodes']

    def rd(q, par):
        n = struct.unpack_from('<H', d, q)[0]
        nodes.append(d[q + 2:q + 2 + n].decode('latin1')); parent.append(par)
        me = len(nodes) - 1
        q += 2 + n + 3 + 64
        nc = struct.unpack_from('<I', d, q)[0]; q += 4
        for _ in range(nc):
            q = rd(q, me)
        return q
    rd(q, -1)
    pieces = []
    q = secs['Pieces'] + 8
    npieces = struct.unpack_from('<I', d, secs['Pieces'] + 4)[0]
    for _ in range(npieces):
        q += 2 + 8 + 4 + 2
        n = struct.unpack_from('<H', d, q)[0]
        pname = d[q + 2:q + 2 + n].decode('latin1'); q += 2 + n
        nt = struct.unpack_from('<I', d, q)[0]; q += 4 + nt * 30
        nv = struct.unpack_from('<I', d, q)[0]; q += 4
        used = set()
        for _ in range(nv):
            nw = struct.unpack_from('<H', d, q)[0]; q += 4
            for _ in range(nw):
                used.add(struct.unpack_from('<I', d, q)[0]); q += 20
            q += 24
        pieces.append((pname, used))
    return nodes, parent, pieces


def subtree(nodes, parent, root):
    s = {root}
    changed = True
    while changed:
        changed = False
        for i, p in enumerate(parent):
            if p in s and i not in s:
                s.add(i); changed = True
    return s


for f in sorted(glob.glob(base + "*/*_pv.ABC")):
    nodes, parent, pieces = load(f)
    low = [n.replace('_zN_', '').lower() for n in nodes]
    root = next((i for i, n in enumerate(low) if n in ('jnt66_1', 'll_arm')), None)
    if root is None:
        continue
    arm = subtree(nodes, parent, root)
    left = [p for p, used in pieces if used and used <= arm]
    mixed = [p for p, used in pieces if used & arm and not used <= arm]
    byname = [p for p, used in pieces if re.match(r'(_zN_)?(l_|ll_)', p, re.I)]
    print('%-24s %2d pieces  left-only %s  mixed %s  by-name %s' % (os.path.basename(f), len(pieces), left, mixed, byname))
