#!/usr/bin/env python3
# mtreport2.py DUMP LIBFILE [depth|files|cats|big]: group memtally's live blocks, but symbols come from the
# link map plus nm of each object file (static functions included), so callers are named right.
# "files" groups by the source file of the first muimaster caller, "cats" by area.
import sys, subprocess, bisect, collections, re, os
dump, lib = sys.argv[1], sys.argv[2]
mode = sys.argv[3] if len(sys.argv) > 3 else '1'
NM = os.environ.get('CROSS', 'm68k-amigaos-') + 'nm'
syms = []  # (addr, name, file)
mapf = lib + '.map'
if not os.path.exists(mapf): mapf = re.sub(r'\.library$', '.map', lib)
if os.path.exists(mapf):
    for l in open(mapf, errors='replace'):
        m = re.match(r'^ \.text\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+\.o\)?)$', l)
        if not m: continue
        base, size, obj = int(m.group(1), 16), int(m.group(2), 16), m.group(3)
        if size == 0: continue
        parts = obj.split('/')
        short = '/'.join(parts[-2:]) if len(parts) > 1 and parts[-2] in ('classes', 'gcc', 'staticlib') else parts[-1]
        if '(' in obj:
            mem = obj.split('(')[1].rstrip(')')
            syms.append((base, 'lib:' + mem, 'lib:' + mem)); continue
        syms.append((base, '<' + short + '>', short))
        for s in subprocess.run([NM, obj], capture_output=True, text=True).stdout.splitlines():
            p = s.split()
            if len(p) >= 3 and p[1] in 'tT' and not p[2].endswith('_desc'):
                syms.append((base + int(p[0], 16), p[2], short))
else:
    for l in subprocess.run([NM, '-n', lib], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) >= 3 and p[1] in 'tT' and not p[2].endswith('_desc'): syms.append((int(p[0], 16), p[2], '?'))
syms.sort()
addrs = [s[0] for s in syms]
M = []; P = []; rows = []
for l in open(dump, errors='replace'):
    p = l.split()
    if not p: continue
    if p[0] == 'M': M.append((int(p[1], 16), int(p[2], 16)))
    elif p[0] == 'P': P.append((int(p[1], 16), int(p[2], 16)))
    elif p[0] == 'T': pass
    else: rows.append([int(x, 16) for x in p])
def look(a):
    if a == 0: return ('-', '-')
    if M and M[0][0] <= a < M[0][1]:
        o = a - M[0][0]; i = bisect.bisect_right(addrs, o) - 1
        return (syms[i][1], syms[i][2]) if i >= 0 else (hex(o), '?')
    for lo, hi in P:
        if lo <= a < hi: return ('PROG', 'PROG')
    if a >= 0xf80000: return ('ROM', 'ROM')
    return ('other', 'other')
CATS = [  # (category, regexp on the first muimaster caller function or file)
    ('objects (instance data via NewObject)', r'DoSuperMethod|Gate_MUI_NewObjectA'),
    ('notifications', r'Notify__MUIM_Notify|notify\.c'),
    ('lists (pools, entries, columns)', r'list\.c|List_|listview'),
    ('text engine / strings', r'textengine|zune_text|StrDup|text\.c|floattext'),
    ('image specs / frames / pens', r'imspec|image|frame|penspec|muigfx'),
    ('windows', r'window\.c|Window_'),
    ('application / ports / config', r'application|Application|configdata|ZuneCreateSharedPort|prefs|SharedPort'),
    ('groups / virtgroups', r'group\.c|virtgroup'),
]
def cat(looked):
    fn, f = looked[0]
    chain = ' '.join(n + ' ' + ff for n, ff in looked)
    if re.search(r'DoSuperMethod', fn) and re.search(r'Notify__OM_NEW|Semaphore__OM_NEW', chain): return CATS[0][0]
    if re.search(r'Notify__MUIM_Notify', chain): return 'notifications'
    if re.search(r'AllocListEntry|List_|list\.o|volumelist|ParseListFormat|SetListSize', chain): return 'lists (pools, entries, columns)'
    if re.search(r'OpenWindow', fn): return 'windows: intuition (smart refresh backing store)'
    if re.search(r'Window', chain): return 'windows'
    if re.search(r'Application|Semaphore|SharedPort|configdata', chain): return 'application / ports / config'
    for c, rx in CATS:
        if re.search(rx, fn) or re.search(rx, f): return c
    return 'other: ' + fn + '@' + f
g = collections.defaultdict(lambda: [0, 0])
for r in rows:
    size, direct, a = r[0], r[1], r[2:]
    looked = [look(x) for x in a[:3]]
    if mode == 'files':
        key = looked[0][1]
    elif mode == 'cats':
        # an allocation made through DoSuperMethodA from Notify's OM_NEW is the object itself
        key = cat(looked)
    else:
        key = ' < '.join(n for n, f in looked[:int(mode) if mode.isdigit() else 3])
    key = ('ROM:' if direct >= 0xf80000 and mode not in ('files', 'cats') else '') + key
    g[key][0] += size; g[key][1] += 1
tot = sum(v[0] for v in g.values())
print('total', tot, 'blocks', len(rows))
for k, v in sorted(g.items(), key=lambda kv: -kv[1][0]):
    print(f'{v[0]:8d} {v[1]:5d}  {k}')
if mode == 'big':
    for r in sorted(rows, key=lambda r: -r[0])[:30]:
        print(r[0], hex(r[1]), ' < '.join(f'{look(x)[0]}@{look(x)[1]}' for x in r[2:5]))
