#!/usr/bin/env python3
"""opbench-compare.py A.txt B.txt [C.txt ...] [--md]: opbench outputs side by side, the median
time per operation in ms, and each later column's ratio to the first (B/A, C/A ...)."""
import re, sys

def load(p):
    d = {}
    for line in open(p, errors='replace'):
        m = re.match(r'(.+?)\s+us/op=\s*([\d.]+)', line)
        if m:
            d[m.group(1).strip()] = float(m.group(2))
        elif re.match(r'(.+?)\s+timeout$', line):
            d[line.split('timeout')[0].strip()] = None
    return d

md = '--md' in sys.argv
files = [a for a in sys.argv[1:] if a != '--md']
data = [load(f) for f in files]
def f(v):
    return '-' if v is None else ('%.1f' % (v / 1000) if v >= 10000 else '%.2f' % (v / 1000))
if md:
    print('| operation | ' + ' | '.join(files) + ' | ' + ' | '.join('/%d' % 1 for _ in files[1:]) + ' |')
    print('|---' * (2 * len(files)) + '|')
for n in data[0]:
    vals = [d.get(n) for d in data]
    rat = ['%.2f' % (v / vals[0]) if v and vals[0] else '-' for v in vals[1:]]
    if md:
        print('| %s | %s | %s |' % (n, ' | '.join(f(v) for v in vals), ' | '.join(rat)))
    else:
        print('%-20s ' % n + ' '.join('%10s' % f(v) for v in vals) + '   ' + ' '.join('%5s' % r for r in rat))
