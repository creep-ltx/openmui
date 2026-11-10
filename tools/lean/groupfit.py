#!/usr/bin/env python3
"""groupfit.py FILE [SPACING]: a model of MUI 3.8's share-out of a group's spare pixels against
rows measured with tests/native/hdist.c (tests/lean/probe/hdist-mui38.txt and the others); prints
every row the model gets wrong. SPACING: the group spacing of the run (4 for the -s4- files).

The model (docs/lean/README.md, branch `leftovers`): fits all but 4 of the 1,318 measured rows, but
built into Zune it made MUI-Demo's Groups window and the Pages demo differ: not the whole rule.
  1. each child trunc(S * weight / all weights), S = the space less twice the minimum of every
     weight-0 child;
  2. eight passes: clamp every child to min/max; each child that can move towards the remainder
     gets trunc(remainder * weight / all weights);
  3. up to eight more: clamp; each such child of weight > 0 one pixel towards the remainder;
  4. clamp; the rest a pixel at a time from the last child back.
Steps 2 and 4 are MUI 3.9's (what Zune does).
"""
import sys

CASES = {  # tests/native/hdist.c: (min, def, max, weight) per child
    "mins": [(10, 10, 9999, 100), (50, 50, 9999, 100), (100, 100, 9999, 100)],
    "mins+4": [(10, 14, 9999, 100), (50, 54, 9999, 100), (100, 104, 9999, 100)],
    "defs": [(0, 10, 9999, 100), (0, 50, 9999, 100), (0, 100, 9999, 100)],
    "bigdefs": [(10, 300, 9999, 100), (50, 300, 9999, 100), (100, 300, 9999, 100)],
    "weights": [(0, 0, 9999, 30), (0, 0, 9999, 100)],
    "weights-min": [(20, 20, 9999, 30), (60, 60, 9999, 100)],
    "w1": [(27, 31, 9999, 1), (48, 52, 9999, 1), (43, 47, 9999, 1), (32, 36, 9999, 1)],
    "maxed": [(10, 10, 40, 100), (50, 50, 9999, 100), (100, 100, 9999, 100)],
    "fix0": [(0, 0, 9999, 30), (2, 2, 2, 0), (0, 0, 9999, 100)],
    "fix100": [(0, 0, 9999, 30), (2, 2, 2, 100), (0, 0, 9999, 100)],
    "fix50w": [(0, 0, 9999, 30), (50, 50, 50, 100), (0, 0, 9999, 100)],
    "fixlast": [(0, 0, 9999, 30), (0, 0, 9999, 100), (2, 2, 2, 100)],
    "fixfirst": [(2, 2, 2, 100), (0, 0, 9999, 30), (0, 0, 9999, 100)],
    "w30-100-100": [(0, 0, 9999, 30), (0, 0, 9999, 100), (0, 0, 9999, 100)],
}


def trunc_div(a, b):
    q = abs(a) // b
    return q if a >= 0 else -q


def share_out(kids, total):
    n, weights = len(kids), sum(k[3] for k in kids)
    space = total - 2 * sum(k[0] for k in kids if k[3] == 0)
    size = [trunc_div(space * k[3], weights) for k in kids]

    def clamp():
        for i, k in enumerate(kids):
            size[i] = min(max(size[i], k[0]), k[2])
        return total - sum(size)

    def movable(r):
        return [i for i, k in enumerate(kids)
                if k[3] and ((r > 0 and size[i] < k[2]) or (r < 0 and size[i] > k[0]))]

    for step in ("share", "pixel"):
        for _ in range(8):
            r = clamp()
            if r == 0:
                return size
            mov = movable(r)
            if not mov:
                break
            for i in mov:
                size[i] += trunc_div(r * kids[i][3], weights) if step == "share" else (1 if r > 0 else -1)
    r = clamp()
    while r:
        moved = False
        for i in reversed(range(n)):
            if r > 0 and size[i] < kids[i][2]:
                size[i] += 1; r -= 1; moved = True
            elif r < 0 and size[i] > kids[i][0]:
                size[i] -= 1; r += 1; moved = True
            if not r:
                break
        if not moved:
            break
    return size


def main():
    spacing = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    rows = wrong = 0
    for line in open(sys.argv[1]):
        p = line.split()
        if len(p) < 3 or p[0] not in CASES:
            continue
        kids, total = CASES[p[0]], int(p[1].rstrip(":"))
        got = [int(x) for x in p[2:]]
        if total == 0:
            continue
        rows += 1
        model = share_out(kids, total - (len(kids) - 1) * spacing)
        if model != got:
            wrong += 1
            print(f"{p[0]:12} {total:4}: MUI 3.8 {got}, model {model}")
    print(f"{rows - wrong} of {rows} rows")


if __name__ == "__main__":
    main()
