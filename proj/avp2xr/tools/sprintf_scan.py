import re, glob, os
root = 'K:/Coding/Aliens-Versus-Predator-2-VR/proj/AVP2/'
decl = re.compile(r'\bchar\s+(\w+)\s*\[\s*(\d+)\s*\]')
call = re.compile(r'\bsprintf\s*\(\s*(\w+)\s*,\s*"((?:[^"\\]|\\.)*)"')
spec = re.compile(r'%[-+ #0]*(\d+|\*)?(?:\.(\d+))?(l|h|I64)?([diouxXeEfgGcs%])')


def min_len(fmt):
    n = 0
    pos = 0
    for m in spec.finditer(fmt):
        n += len(fmt[pos:m.start()].encode().decode('unicode_escape'))
        width = int(m.group(1)) if m.group(1) and m.group(1) != '*' else 0
        prec = int(m.group(2)) if m.group(2) else None
        c = m.group(4)
        if c == 'f':
            k = 2 + (6 if prec is None else prec) - (1 if prec == 0 else 0)
        elif c in 'eE':
            k = 12
        elif c == '%':
            k = 1
        elif c == 's':
            k = 0
        else:
            k = 1
        n += max(k, width)
        pos = m.end()
    n += len(fmt[pos:].encode().decode('unicode_escape'))
    return n + 1   # the terminator


for f in sorted(glob.glob(root + 'ClientShellDLL/*.cpp') + glob.glob(root + 'Shared/*.cpp')):
    lines = open(f, errors='replace').read().split('\n')
    sizes = {}
    for i, l in enumerate(lines):
        for m in decl.finditer(l):
            sizes[m.group(1)] = (int(m.group(2)), i)
        for m in call.finditer(l):
            buf, fmt = m.group(1), m.group(2)
            if buf in sizes:
                size, di = sizes[buf]
                need = min_len(fmt)
                if need > size:
                    print('%s:%d  char %s[%d] but "%s" needs at least %d' % (os.path.basename(f), i + 1, buf, size, fmt, need))
