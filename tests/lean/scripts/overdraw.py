#!/usr/bin/env python3
"""overdraw.py OUT LIBDIR OP [N] [W H]: how much of what one operation fills is painted again.

OUT is an opbench run with "count who=RectFill" (tests/native/syscount.h records each top-level
RectFill's rectangle and callers), LIBDIR the library's build folder (link map and objects, as for
syscount-who.py). The first N records of OP (default: its top-level RectFill count per operation,
from the "calls" line: one run of the operation) are replayed on a W x H map (default 1024 x 1024,
window coordinates): the total area filled, the area covered (each pixel once) and, per caller
(the RectFill's caller and the two functions above it), the pixels it filled that a later fill of
the same operation covered again ("wasted": what MUI need not have painted) and the pixels it
painted over an earlier fill ("over")."""
import collections, os, re, subprocess, sys

out, libdir, only = sys.argv[1], sys.argv[2], sys.argv[3]
N = int(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] != '-' else None
W = int(sys.argv[5]) if len(sys.argv) > 5 else 1024
H = int(sys.argv[6]) if len(sys.argv) > 6 else 1024
DEPTH = int(os.environ.get('DEPTH', '3'))

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
    if i < 0 or a - syms[i][0] > 0x4000:
        return '?'
    return syms[i][1].lstrip('_')

base, recs, top = None, [], None
for line in open(out, errors='replace'):
    if line.startswith('whobase '):
        base = int(line.split()[1], 16) - gate
    elif line.startswith('calls ' + only + ' |'):
        m = re.search(r'\| RectFill ([\d.]+) ', line)
        top = int(round(float(m.group(1)))) if m else None
    elif line.startswith('who ' + only + ' '):
        m = re.match(r'who (.*?) a=(\d+) r=(-?\d+),(-?\d+),(-?\d+),(-?\d+)((?: [0-9a-f]{8})+)$',
                     line.rstrip())
        if not m:
            continue
        r = tuple(int(m.group(i)) for i in range(3, 7))
        ads = [int(x, 16) for x in m.group(7).split()]
        recs.append((r, ' < '.join(name(a - base) for a in ads[:DEPTH])))
n = N or top or len(recs)
recs = recs[:n]
owner = [-1] * (W * H)            # which record painted each pixel last
wasted = collections.Counter()
over = collections.Counter()
area = collections.Counter()
calls = collections.Counter()
total = 0
for i, ((x0, y0, x1, y1), who) in enumerate(recs):
    x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, W - 1), min(y1, H - 1)
    calls[who] += 1
    for y in range(y0, y1 + 1):
        row = y * W
        for x in range(x0, x1 + 1):
            o = owner[row + x]
            if o >= 0:
                wasted[recs[o][1]] += 1
                over[who] += 1
            owner[row + x] = i
            area[who] += 1
            total += 1
covered = sum(1 for o in owner if o >= 0)
print('%s: %d RectFill, %d pixels filled, %d covered, %d painted again (%.1f%%)'
      % (only, len(recs), total, covered, total - covered,
         100.0 * (total - covered) / max(total, 1)))
print('%8s %8s %8s %6s  caller' % ('filled', 'wasted', 'over', 'calls'))
for who, a in area.most_common():
    print('%8d %8d %8d %6d  %s' % (a, wasted[who], over[who], calls[who], who))
