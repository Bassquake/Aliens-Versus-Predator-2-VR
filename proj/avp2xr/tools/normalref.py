"""Makes copies of first-person weapon models whose lighting reference (the command string's
NormalRef node) is on the left arm, pointing it at a node the left hand doesn't move instead.

The engine lights a model from its NormalRef node (or, without one, what looks like the first node
after the root). In VR the left arm follows the left controller, so where that node was on the left
arm, the light on the whole model changed as the hands moved relative to each other: the smartgun
names its left forearm, and the Alien claw models name nothing and start with the left arm.

The copies go under deploy/vrrez/Models/..., which install.bat copies into the game's vrrez folder
(it overrides the game's own files, like the VR cshell.dll). Usage: python tools/normalref.py
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from abcfile import Abc

GAME = r'C:\Program Files (x86)\Fox\Aliens vs. Predator 2\AVP2'
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'deploy', 'vrrez')

# model -> node to light it from (the gun body; for the Aliens the right forearm, as the other
# weapons end up with)
MODELS = {
    r'Models\Weapons\Marine\mSmartgun_pv.ABC': 'bmerge1_2',
    r'Models\Weapons\Alien\aClaws_pv.ABC': 'rl_arm',
    r'Models\Weapons\Alien\aEat_pv.ABC': 'rl_arm',
    r'Models\Weapons\Alien\aFacehug_pv.ABC': 'rl_arm',
    r'Models\Weapons\Alien\aPounceJump_pv.ABC': 'rl_arm',
    r'Models\Weapons\Alien\aPounce_pv.ABC': 'rl_arm',
    r'Models\Weapons\Alien\aTear_pv.ABC': 'rl_arm',
}


def node_names(abc):
    import struct
    d = dict(abc.sections)['Nodes']
    names = []

    def rd(p):
        n = struct.unpack_from('<H', d, p)[0]
        names.append(d[p + 2:p + 2 + n].decode('latin1'))
        p += 2 + n + 3 + 64
        nc = struct.unpack_from('<I', d, p)[0]
        p += 4
        for _ in range(nc):
            p = rd(p)
        return p
    rd(0)
    return names


for rel, node in MODELS.items():
    src = os.path.join(GAME, rel)
    data = open(src, 'rb').read()
    abc = Abc(src)
    assert abc.build() == data, 'not a model layout this tool understands: ' + rel
    assert node in node_names(abc), '%s has no node %s' % (rel, node)
    before = abc.command
    if re.search(r'NormalRef\s+\S+', abc.command):
        abc.command = re.sub(r'NormalRef\s+[^\s;]+', 'NormalRef ' + node, abc.command)
    else:
        abc.command = abc.command.rstrip() + ' NormalRef ' + node + ';'
    # the header's string total, in case the engine sizes a buffer by it
    grow = len(abc.command) - len(before)
    if grow > 0:
        abc.head_ints[13] += grow
    out = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, 'wb').write(abc.build())
    check = Abc(out)
    assert check.command == abc.command
    print('%s: %r -> %r' % (rel, before, abc.command))
