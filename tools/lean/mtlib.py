#!/usr/bin/env python3
# mtlib.py DUMP [DUMP2...]: memtally dumps (runall/lib, with L and K lines) grouped by task, by the
# first caller outside ROM (named by the library whose jump-table span holds it, PROG, or with
# SYMLIB=<Zune muimaster.library built with symbols, its .map next to it> the Zune function),
# task and first caller together, the whole caller chain, and a block size histogram; several
# dumps side by side (Zune and MUI 3.8: MUI 3.8 shows as muimaster.library, black box).
# Library names for RAM addresses are a guess (the span plus 64 KB): code that LoadSeg loads
# shows under whatever library lies just below it.
import sys, collections, subprocess, bisect, os
SYMLIB = os.environ.get('SYMLIB')   # Zune's muimaster.library with symbols: name its functions
SY = []
if SYMLIB:
    import re
    NM = os.path.expanduser('~/opt/amiga-gcc16/bin/m68k-amigaos-nm')
    mapf = SYMLIB + '.map'
    for l in open(mapf, errors='replace'):
        m = re.match(r'^ \.text\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+\.o\)?)$', l)
        if not m: continue
        base, size, obj = int(m.group(1), 16), int(m.group(2), 16), m.group(3)
        if size == 0: continue
        if '(' in obj:
            SY.append((base, 'lib:' + obj.split('(')[1].rstrip(')'))); continue
        SY.append((base, '<' + os.path.basename(obj) + '>'))
        for q in subprocess.run([NM, obj], capture_output=True, text=True).stdout.splitlines():
            q = q.split()
            if len(q) >= 3 and q[1] in 'tT' and not q[2].endswith('_desc'):
                SY.append((base + int(q[0], 16), q[2]))
    SY.sort()
SA = [x[0] for x in SY]
def mfunc(a, M):
    if not SY or not M or not (M[0][0] <= a < M[0][1]): return 'muimaster'
    i = bisect.bisect_right(SA, a - M[0][0]) - 1
    return 'mm:' + SY[i][1] if i >= 0 else 'muimaster'

def load(fn):
    M = []; P = []; L = []; K = {}; rows = []
    for l in open(fn, errors='replace'):
        p = l.split()
        if not p: continue
        if p[0] == 'M': M.append((int(p[1], 16), int(p[2], 16)))
        elif p[0] == 'P': P.append((int(p[1], 16), int(p[2], 16)))
        elif p[0] == "L":
            lo, hi = int(p[1], 16), int(p[2], 16)
            if hi - lo < 0x40000: L.append((lo, hi, " ".join(p[3:])))
        elif p[0] == 'K': K[int(p[1], 16)] = ' '.join(p[2:])
        elif p[0] == 'T': pass
        else: rows.append([int(x, 16) for x in p])
    L.sort()
    return M, P, L, K, rows

SLACK = 0x10000
def owner(a, M, P, L):
    if a == 0: return None
    for lo, hi in M:
        if lo <= a < hi: return mfunc(a, M)
    for lo, hi in P:
        if lo <= a < hi: return 'PROG'
    if a >= 0xf80000:
        for lo, hi, n in L:
            if lo <= a <= hi + 0x800 and lo >= 0xf80000: return 'ROM:' + n
        return 'ROM'
    best = None
    for lo, hi, n in L:
        if lo <= a <= hi + SLACK and (best is None or lo > best[0]): best = (lo, n)
    return best[1] if best else 'RAM?'

def analyse(fn):
    M, P, L, K, rows = load(fn)
    bytask = collections.Counter(); byown = collections.Counter(); nown = collections.Counter()
    hist = collections.Counter(); chain = collections.Counter(); tf = collections.Counter()
    for r in rows:
        size, direct = r[0], r[1]; a = r[2:5]; task = r[5] if len(r) > 5 else 0
        tn = K.get(task, hex(task))
        bytask[tn] += size
        os_ = [owner(x, M, P, L) for x in [direct] + a]
        os_ = [o for o in os_ if o]
        first = next((o for o in os_ if not o.startswith('ROM')), 'ROM')
        byown[first] += size; nown[first] += 1
        chain[' < '.join(dict.fromkeys(os_))] += size
        tf[tn[:14] + ' | ' + first] += size
        b = 8
        while b < size: b *= 2
        hist[b] += 1
    return bytask, byown, nown, hist, chain, sum(r[0] for r in rows), len(rows), tf

res = [analyse(f) for f in sys.argv[1:]]
for name, idx in (('tasks', 0), ('first caller', 1), ('task | first caller', 7), ('chains', 4)):
    print('==', name)
    keys = collections.Counter()
    for r in res: keys.update(r[idx])
    for k, _ in keys.most_common(40):
        print('  %-60s' % k[:60] + ''.join('%10d' % r[idx].get(k, 0) for r in res))
print('== size histogram (blocks <= N bytes)')
ks = sorted(set().union(*[r[3].keys() for r in res]))
for k in ks: print('  %8d' % k + ''.join('%8d' % r[3].get(k, 0) for r in res))
print('== total', ' '.join('%d bytes / %d blocks' % (r[5], r[6]) for r in res))
