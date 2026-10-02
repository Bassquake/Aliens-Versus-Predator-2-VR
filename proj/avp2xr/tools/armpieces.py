"""Lists a first-person model's pieces with how much of each is on the left arm's nodes, to see which
pieces the VR left-arm copy should show (VRMgr.cpp s_ArmPieces). Usage: python tools/armpieces.py <model.abc>
(a path under the game's AVP2 folder, e.g. Models\\Weapons\\Predator\\pWristBlades_pv.ABC)
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from abcfile import Abc
from abcpieces import read_pieces

GAME = r'C:\Program Files (x86)\Fox\Aliens vs. Predator 2\AVP2'
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'deploy', 'vrrez')


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


if __name__ == '__main__':
    rel = sys.argv[1]
    src = os.path.join(OUT, rel) if os.path.exists(os.path.join(OUT, rel)) else os.path.join(GAME, rel)
    abc = Abc(src)
    names, parents = node_tree(abc)
    arm = left_arm_nodes(names, parents)
    print(src)
    print('left arm nodes:', ' '.join(names[i] for i in sorted(arm)))
    total, pieces = read_pieces(dict(abc.sections)['Pieces'])
    for pc in pieces:
        left = [vertex_on_arm(v, arm) for v in pc.verts]
        nodes = set()
        for v in pc.verts:
            for w in v[1]:
                nodes.add(names[struct.unpack_from('<I', w, 0)[0]])
        print('%-22s %4d verts, %4d on the left arm, %4d tris; nodes: %s' % (
            pc.name, len(pc.verts), sum(left), len(pc.tris), ' '.join(sorted(nodes))))
