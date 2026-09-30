"""Splits the left arm out of first-person weapon models whose left and right hands share a piece.

In VR the left arm follows the left controller. A model whose left arm has pieces of its own draws
it as a second copy of the model (lit from its own side); one where both hands are in one piece can
only move the arm's nodes, and then the whole model is lit from one reference, so the left hand's
light changes with the right hand. This writes copies of such models with each shared piece split
in two: the triangles on the left arm's nodes into a new piece "<name>_left", the rest kept.

The copies go under deploy/vrrez/Models/..., which install.bat copies into the game's vrrez folder
(overriding the game's own files). Usage: python tools/splitarms.py
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from abcfile import Abc
from abcpieces import read_pieces, write_pieces

GAME = r'C:\Program Files (x86)\Fox\Aliens vs. Predator 2\AVP2'
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'deploy', 'vrrez')

MODELS = [
    r'Models\Weapons\Marine\mKnife_pv.ABC',
    r'Models\Weapons\Marine\mPistol_pv.ABC',
    r'Models\Weapons\Marine\mGrenade_pv.ABC',
    r'Models\Weapons\Marine\mFlamer_pv.ABC',
    r'Models\Weapons\Marine\mSadar_pv.ABC',
    r'Models\Weapons\Marine\mPulserifle_pv.ABC',
    r'Models\Weapons\Predator\pSpearGun_pv.ABC',
    r'Models\Weapons\Predator\pDisc_pv.ABC',
    r'Models\Weapons\Predator\pNetGun_pv.ABC',
    r'Models\Weapons\Predator\pEnergySift_pv.ABC',
]


def node_tree(abc):
    d = dict(abc.sections)['Nodes']
    names, parents = [], []

    def rd(p, parent):
        n = struct.unpack_from('<H', d, p)[0]
        names.append(d[p + 2:p + 2 + n].decode('latin1'))
        parents.append(parent)
        me = len(names) - 1
        p += 2 + n + 3 + 64
        nc = struct.unpack_from('<I', d, p)[0]
        p += 4
        for _ in range(nc):
            p = rd(p, me)
        return p
    rd(0, -1)
    return names, parents


def left_arm_nodes(names, parents):
    root = next(i for i, n in enumerate(names) if n.replace('_zN_', '').lower() in ('jnt66_1', 'll_arm'))
    arm = {root}
    grown = True
    while grown:
        grown = False
        for i, par in enumerate(parents):
            if par in arm and i not in arm:
                arm.add(i)
                grown = True
    return arm


def vertex_on_arm(vert, arm):
    total = on = 0.0
    for w in vert[1]:
        node = struct.unpack_from('<I', w, 0)[0]
        weight = struct.unpack_from('<f', w, 16)[0]
        total += weight
        if node in arm:
            on += weight
    return total > 0 and on / total > 0.5


def sub_piece(pc, tris, name):
    new = type(pc)()
    new.head = pc.head
    new.name = name
    remap = {}
    new.verts = []
    new.tris = []
    for tri in tris:
        out = []
        for u, v, i in tri:
            if i not in remap:
                remap[i] = len(new.verts)
                new.verts.append(pc.verts[i])
            out.append((u, v, remap[i]))
        new.tris.append(out)
    return new


for rel in MODELS:
    src = os.path.join(OUT, rel) if os.path.exists(os.path.join(OUT, rel)) else os.path.join(GAME, rel)
    data = open(src, 'rb').read()
    abc = Abc(src)
    assert abc.build() == data, 'not a model layout this tool understands: ' + rel
    names, parents = node_tree(abc)
    arm = left_arm_nodes(names, parents)
    sec = dict(abc.sections)
    total, pieces = read_pieces(sec['Pieces'])
    assert write_pieces(pieces)[0] == sec['Pieces']

    new_pieces = []
    added = []
    for pc in pieces:
        left = [vertex_on_arm(v, arm) for v in pc.verts]
        if not any(left) or all(left) or pc.name.endswith('_left'):
            new_pieces.append(pc)
            continue
        ltris, rtris, bridging = [], [], 0
        for tri in pc.tris:
            n = sum(left[i] for _, _, i in tri)
            if 0 < n < 3:
                bridging += 1
            (ltris if n >= 2 else rtris).append(tri)
        new_pieces.append(sub_piece(pc, rtris, pc.name))
        lp = sub_piece(pc, ltris, pc.name + '_left')
        new_pieces.append(lp)
        added.append(lp.name)
        print('%s: piece %s split: %d triangles to %s, %d kept, %d bridging both hands' % (
            rel, pc.name, len(ltris), lp.name, len(rtris), bridging))
    if not added:
        print('%s: nothing to split' % rel)
        continue
    assert len(new_pieces) <= 32

    piece_data, weights = write_pieces(new_pieces)
    for i, (name, _) in enumerate(abc.sections):
        if name == 'Pieces':
            abc.sections[i][1] = piece_data
    h = abc.head_ints
    h[4] = len(new_pieces)
    h[6] = sum(len(p.tris) for p in new_pieces)
    h[7] = sum(len(p.verts) for p in new_pieces)
    h[8] = weights
    h[12] += len(added)
    h[13] += sum(len(n) for n in added)

    out = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, 'wb').write(abc.build())
    # read it back
    check = Abc(out)
    t2, p2 = read_pieces(dict(check.sections)['Pieces'])
    assert [p.name for p in p2] == [p.name for p in new_pieces]
    print('   written, %d pieces: %s' % (len(p2), ' '.join(p.name for p in p2)))
