# Independent reader for LithTech attribute files, to cross-check the rewritten ButeMgr.
# Prints the same lines as butetest.exe: model/skin keys resolved through the Parent chain.
import re, sys

text = open(sys.argv[1], 'rb').read().decode('latin-1')
text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)       # block comments
tags, order, cur = {}, [], None
for raw in text.splitlines():
    line = re.sub(r'//.*$', '', raw).strip()
    if not line:
        continue
    m = re.match(r'^\[([^\]]+)\]$', line)
    if m:
        cur = m.group(1)
        if cur not in tags:
            tags[cur] = {}
            order.append(cur)
        continue
    m = re.match(r'^([A-Za-z0-9_]+)\s*=\s*(.*)$', line)
    if m and cur is not None:
        key, val = m.group(1), m.group(2).strip()
        if val.startswith('"'):
            val = val[1:val.rindex('"')] if val.count('"') >= 2 else val[1:]
        tags[cur].setdefault(key, val)              # first definition wins

lower = {t.lower(): t for t in tags}

def get(tag, key, depth=0):
    if not tag or depth > 20:
        return ''
    t = lower.get(tag.lower())
    if t is None:
        return ''
    if key in tags[t]:
        return tags[t][key]
    parent = tags[t].get('Parent', '')
    if parent.lower() == tag.lower():
        return ''
    return get(parent, key, depth + 1)

for t in order:
    model = get(t, 'DefaultModel')
    if not model:
        continue
    skins = ' '.join('s%d=%s' % (i, get(t, 'DefaultSkin%d' % i)) for i in range(4))
    print('[%s] model=%s%s skindir=%s %s' % (t, get(t, 'DefaultModelDir'), model, get(t, 'DefaultSkinDir'), skins))
