#!/usr/bin/env python3
"""syscount-who.py OUT LIBDIR [OP]: the callers of one OS function in OpenMUI, from an opbench
run with "count who=NAME" (tests/native/syscount.h) and the library's build folder LIBDIR (its
muimaster.library.map and objects). Each "who" line is a return address and the return
addresses into the library found further up the stack (a guess, the first may be stale); they
are named by the link map's .text lines and each object's local symbols (nm -n), as
pcsample-full.py does, and counted per chain. OP: only that operation."""
import collections, os, re, subprocess, sys

out, libdir = sys.argv[1], sys.argv[2]
only = sys.argv[3] if len(sys.argv) > 3 else None
syms, gate = [], None
for line in open(os.path.join(libdir, 'muimaster.library.map')):
    m = re.match(r'\s*\.text\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+\.o)$', line)
    if m and os.path.exists(m.group(3)):
        base = int(m.group(1), 16)
        nm = subprocess.run(['m68k-amigaos-nm', '-n', m.group(3)], capture_output=True,
                            text=True).stdout
        for l in nm.splitlines():
            p = l.split()
            if len(p) == 3 and p[1] in 'tT':
                syms.append((base + int(p[0], 16), p[2]))
    m = re.match(r'\s+0x([0-9a-f]+)\s+Gate_MUI_NewObjectA$', line)
    if m:
        gate = int(m.group(1), 16)
syms.sort()
addrs = [a for a, _ in syms]

def name(a):
    import bisect
    i = bisect.bisect_right(addrs, a) - 1
    if i < 0:
        return '?%x' % a
    return '%s+%x' % (syms[i][1].lstrip('_'), a - syms[i][0])

base = None
chains = collections.Counter()
areas = collections.Counter()
for line in open(out, errors='replace'):
    if line.startswith('whobase '):
        base = int(line.split()[1], 16) - gate
    elif line.startswith('who '):
        m = re.match(r'who (.*?)(?: a=(\d+))?((?: [0-9a-f]{8})+)$', line.rstrip())
        op, ads = m.group(1), [int(x, 16) for x in m.group(3).split()]
        if only and op != only:
            continue
        key = (op,) + tuple(name(a - base) for a in ads)
        chains[key] += 1
        areas[key] += int(m.group(2) or 0)
for c, n in chains.most_common():
    a = (' pixels=%d' % areas[c]) if areas[c] else ''
    print('%5d  %-18s %s%s' % (n, c[0], ' < '.join(c[1:]), a))
