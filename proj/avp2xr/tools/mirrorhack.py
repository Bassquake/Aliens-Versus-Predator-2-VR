"""Mirrors the Marine hacking device's first-person model left to right for VR.

In the game the right hand holds the device and the left hand the cable's plug. In VR the device is
wanted in the left hand and the plug in the right, so this writes a mirrored copy of the model
(across its X, the side-to-side axis): vertex offsets, positions and normals, node matrices,
animation keys and child-model relations mirrored, triangles turned the other way round (so they
still face out), and the two arms' node names swapped, so the arm that now holds the device (on the
left) is the one VRMgr moves to the left controller. The screen pieces' texture is flipped back so
their writing isn't mirrored. VRMgr moves the device with the left arm and bends the cable to the
plug (VRMgr::HackCableNodeControl).

The copy goes under deploy/vrrez/Models/..., which Install_avp2vr.bat copies into the game's vrrez
folder (overriding the game's own file).
Usage: python tools/mirrorhack.py <mHackingtool_pv.ABC from AVP2.REZ>
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from abcfile import Abc
from abcpieces import read_pieces, write_pieces

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'deploy', 'vrrez',
                   'Models', 'Weapons', 'Marine', 'mHackingtool_pv.ABC')

# The right arm's nodes and the left arm's, which change places
SWAP = [('Forearm_r1', 'jnt66_1'), ('Wrist_r1', 'jnt66_2'),
        ('Thumb_r1', 'jnt67_1'), ('Thumb_r2', 'jnt67_2'), ('Thumb_r3', 'jnt67_3')]
for finger in ('Pointer', 'Middle', 'Ring', 'Pinky'):
    for i in range(1, 5):
        SWAP.append(('%s_r%d' % (finger, i), '%s_r%d' % (finger, i + 4)))
RENAME = {}
for a, b in SWAP:
    RENAME[a] = b
    RENAME[b] = a

# Pieces whose texture is writing (the device's screens)
SCREENS = ('Display_zTex2', 'Looper_zTex3')


def mirror_matrix(m):
    # M m M with M = diag(-1, 1, 1, 1): element (i, j) changes sign when exactly one of i, j is 0
    return [v * (-1.0 if (i == 0) != (j == 0) else 1.0) for k, v in enumerate(m) for i, j in [divmod(k, 4)]]


def mirror_pos_quat(data, p):
    x, y, z, qx, qy, qz, qw = struct.unpack_from('<7f', data, p)
    return struct.pack('<7f', -x, y, z, qx, -qy, -qz, qw)


def mirror_nodes(d):
    out = bytearray()
    names = []

    def rd(p):
        n = struct.unpack_from('<H', d, p)[0]
        name = d[p + 2:p + 2 + n].decode('latin1')
        names.append(name)
        new = RENAME.get(name, name).encode('latin1')
        out.extend(struct.pack('<H', len(new)) + new)
        p += 2 + n
        out.extend(d[p:p + 3])  # index, flags
        p += 3
        m = struct.unpack_from('<16f', d, p)
        out.extend(struct.pack('<16f', *mirror_matrix(m)))
        p += 64
        nc = struct.unpack_from('<I', d, p)[0]
        out.extend(d[p:p + 4])
        p += 4
        for _ in range(nc):
            p = rd(p)
        return p
    end = rd(0)
    # then the weight sets: u32 count (0 here)
    assert end + 4 == len(d) and struct.unpack_from('<I', d, end)[0] == 0, (end, len(d))
    out.extend(d[end:])
    missing = [a for a in RENAME if a not in names]
    assert not missing, missing
    return bytes(out), len(names)


def mirror_pieces(d):
    total, pieces = read_pieces(d)
    for pc in pieces:
        flip_u = pc.name in SCREENS
        tris = []
        for t in pc.tris:
            t = [t[0], t[2], t[1]]
            if flip_u:
                t = [(1.0 - u, v, i) for u, v, i in t]
            tris.append(t)
        pc.tris = tris
        verts = []
        for pad, weights, rest in pc.verts:
            nw = []
            for w in weights:
                node, ox, oy, oz, wt = struct.unpack('<I4f', w)
                nw.append(struct.pack('<I4f', node, -ox, oy, oz, wt))
            px, py, pz, nx, ny, nz = struct.unpack('<6f', rest)
            verts.append((pad, nw, struct.pack('<6f', -px, py, pz, -nx, ny, nz)))
        pc.verts = verts
    data, total2 = write_pieces(pieces)
    assert total2 == total
    return data


def mirror_animation(d, nnodes):
    out = bytearray(d)
    p = 0
    nanims = struct.unpack_from('<I', d, p)[0]
    p += 4
    for _ in range(nanims):
        p += 12
        n = struct.unpack_from('<H', d, p)[0]
        p += 2 + n
        compression = struct.unpack_from('<i', d, p)[0]
        assert compression == -1, 'compressed animation: not handled'
        p += 8
        nk = struct.unpack_from('<I', d, p)[0]
        p += 4
        for _ in range(nk):
            p += 4
            n = struct.unpack_from('<H', d, p)[0]
            p += 2 + n
        for _ in range(nnodes * nk):
            out[p:p + 28] = mirror_pos_quat(d, p)
            p += 28
    assert p == len(d), (p, len(d))
    return bytes(out)


def mirror_child_models(d, nnodes):
    # u32 child model count (1: the model itself), u32 unknown, then each node's relation (position,
    # rotation) to it
    out = bytearray(d)
    assert struct.unpack_from('<I', d, 0)[0] == 1 and len(d) == 8 + nnodes * 28, 'child models: not handled'
    p = 8
    for _ in range(nnodes):
        out[p:p + 28] = mirror_pos_quat(d, p)
        p += 28
    return bytes(out)


def main():
    a = Abc(sys.argv[1])
    sec = {name: i for i, (name, _) in enumerate(a.sections)}
    nodes, nnodes = mirror_nodes(a.sections[sec['Nodes']][1])
    assert nnodes == a.head_ints[3]
    a.sections[sec['Nodes']][1] = nodes
    a.sections[sec['Pieces']][1] = mirror_pieces(a.sections[sec['Pieces']][1])
    a.sections[sec['Animation']][1] = mirror_animation(a.sections[sec['Animation']][1], nnodes)
    a.sections[sec['ChildModels']][1] = mirror_child_models(a.sections[sec['ChildModels']][1], nnodes)
    assert struct.unpack_from('<I', a.sections[sec['Sockets']][1], 0)[0] == 0, 'sockets: not handled'
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, 'wb').write(a.build())
    print('wrote', OUT)


if __name__ == '__main__':
    main()
