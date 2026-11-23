#!/usr/bin/env python3
"""gdist.py FILE...: MUI 3.8's spare-pixel rule as OpenMUI has it since
Compatibility round 5 and 2026-11-23 (group.c, LayDistribute), checked against gdist.c's
output (lines "H|V spacing length: min/def/max/weight ... = size ...", and
"C<columns> spacing width: ..." for a ColGroup) and hdist.c's files. Prints how
many lines the rule gets right. MUI 3.8's own files: gdist-*-mui38.txt
(gdist-cases.txt the case list, random seeds 7 and 11-13), hdist-*-mui38.txt."""
import re, sys

def rule(kids, length, spacing):
    """kids: (min, def, max, weight); the sizes the rule gives"""
    total = length - (len(kids) - 1) * spacing
    weight = sum(k[3] for k in kids)
    if weight == 0:
        return [k[0] for k in kids]
    s = [total * k[3] // weight for k in kids]
    def clamp():
        r = total
        for i, k in enumerate(kids):
            s[i] = max(k[0], min(k[2], s[i]))
            r -= s[i]
        return r
    while True:
        rem = clamp()
        if rem == 0:
            return s
        moved = False
        for i, k in enumerate(kids):
            if (rem > 0 and s[i] >= k[2]) or (rem < 0 and s[i] <= k[0]):
                continue
            d = abs(rem) * k[3] // weight * (1 if rem > 0 else -1)
            if d:
                s[i] += d
                moved = True
        if not moved:
            break
    # fewer pixels left than children that can grow: each gets one, the
    # excess is taken back below, from the first child on (2026-11-23)
    can = [i for i, k in enumerate(kids) if s[i] < k[2]]
    if 0 < rem < len(can):
        for i in can:
            s[i] += 1
        rem -= len(can)
    while rem:
        moved = False
        order = range(len(kids) - 1, -1, -1) if rem > 0 else range(len(kids))
        for i in order:
            if rem == 0:
                break
            if rem > 0 and s[i] < kids[i][2]:
                s[i] += 1; rem -= 1; moved = True
            elif rem < 0 and s[i] > kids[i][0]:
                s[i] -= 1; rem += 1; moved = True
        if not moved:
            break
    return s

def grid(cols, kids, width, spacing):
    rows = len(kids) // cols
    cs = []
    for j in range(cols):
        ks = [kids[i * cols + j] for i in range(rows)]
        mn = max(k[0] for k in ks)
        cs.append((mn, 0, max(mn, min(k[2] for k in ks)), sum(k[3] for k in ks) // rows))
    w = rule(cs, width, spacing)
    return [max(k[0], min(k[2], w[i % cols])) for i, k in enumerate(kids)]

HDIST = {"mins": [(10,10,9999,100),(50,50,9999,100),(100,100,9999,100)],
    "mins+4": [(10,14,9999,100),(50,54,9999,100),(100,104,9999,100)],
    "defs": [(0,10,9999,100),(0,50,9999,100),(0,100,9999,100)],
    "bigdefs": [(10,300,9999,100),(50,300,9999,100),(100,300,9999,100)],
    "weights": [(0,0,9999,30),(0,0,9999,100)], "weights-min": [(20,20,9999,30),(60,60,9999,100)],
    "w1": [(27,31,9999,1),(48,52,9999,1),(43,47,9999,1),(32,36,9999,1)],
    "maxed": [(10,10,40,100),(50,50,9999,100),(100,100,9999,100)],
    "fix0": [(0,0,9999,30),(2,2,2,0),(0,0,9999,100)], "fix100": [(0,0,9999,30),(2,2,2,100),(0,0,9999,100)],
    "fix50w": [(0,0,9999,30),(50,50,50,100),(0,0,9999,100)], "fixlast": [(0,0,9999,30),(0,0,9999,100),(2,2,2,100)],
    "fixfirst": [(2,2,2,100),(0,0,9999,30),(0,0,9999,100)], "w30-100-100": [(0,0,9999,30),(0,0,9999,100),(0,0,9999,100)]}

for fn in sys.argv[1:]:
    good = n = 0
    sp4 = "-s4" in fn
    for line in open(fn):
        m = re.match(r"([HVC])(\d?) (\d+) (-?\d+): (.*) = (.*)", line)
        if m:
            kids = [tuple(int(x) for x in k.split("/")) for k in m[5].split()]
            want = [int(x) for x in m[6].split()]
            got = grid(int(m[2]), kids, int(m[4]), int(m[3])) if m[1] == "C" \
                else rule(kids, int(m[4]), int(m[3]))
        else:
            m = re.match(r"(\S+)\s+(-?\d+):(.*)", line)
            if not m or m[1] not in HDIST:
                continue
            want = [int(x) for x in m[3].split()]
            got = rule(HDIST[m[1]], int(m[2]), 4 if sp4 else 0)
        n += 1
        good += got == want
    print("%s: %d of %d lines as the rule" % (fn, good, n))
