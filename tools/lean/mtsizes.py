#!/usr/bin/env python3
# mtsizes.py DUMP PATTERN...: per pattern (a substring of a caller's name in the chain), the bytes and
# the block size histogram of the blocks whose chain names it. SYMLIB as for mtlib.py.
import sys, collections, os
fn, pats = sys.argv[1], sys.argv[2:]
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'mtlib.py')).read().split('res = [analyse')[0]
sys.argv = ['mtlib.py']
exec(src)
M, P, L, K, rows = load(fn)
for pat in pats:
    c = collections.Counter()
    for r in rows:
        ch = [owner(x, M, P, L) for x in r[1:5]]
        if any(o and pat in o for o in ch):
            c[r[0]] += 1
    print('%-24s %7d bytes %5d blocks  %s' % (pat, sum(k * v for k, v in c.items()), sum(c.values()),
          sorted(c.items(), key=lambda x: -x[0] * x[1])[:14]))
if os.environ.get('SIZE'):
    want = int(os.environ['SIZE'])
    ch = collections.Counter()
    for r in rows:
        if r[0] == want:
            ch[' < '.join(o for o in (owner(x, M, P, L) for x in r[1:5]) if o)] += 1
    for k, v in ch.most_common(8): print('  %4d  %s' % (v, k))
