#!/usr/bin/env python3
"""suite-compare.py A B [OUT.html]: compare two real-program suite runs side by side.

A and B are result folders of suite.sh ($ZWORK/suite-real/<tag>, e.g. zune and mui38). Writes an
HTML contact sheet (default: <parent>/compare-<A>-<B>.html next to them) with, per program, both
screenshots (main window and each extra window), the windows with their sizes and the
differences, both statuses and the memory lost on the second run. Prints a plain summary: every
program that behaves differently in A than in B.
"""
import html
import os
import re
import sys


def load(d):
    res = {}
    p = os.path.join(d, "results.tsv")
    if not os.path.exists(p):
        return res
    for line in open(p, encoding="utf-8", errors="replace"):
        f = line.rstrip("\n").split("\t")
        if len(f) < 5:
            continue
        pid, status, left, lost, wl = f[:5]
        parts = wl.split("|")
        main = parse_wins(parts[0])
        extras = []
        for x in parts[1:]:
            m = re.match(r"extra(\d+)=(.*?):(.*)$", x)
            if m:
                extras.append((m.group(2), parse_wins(m.group(3))))
        inuse = f[5] if len(f) > 5 else "?"  # memory held by the 2nd run (newer suite runs)
        inuse1 = f[6] if len(f) > 6 else "?"  # the same for the 1st run, from a fresh boot
        res[pid] = dict(status=status, left=left, lost=lost, main=main, extras=extras, inuse=inuse,
                        inuse1=inuse1)
    return res


def parse_wins(s):
    out = []
    for w in s.split(";"):
        if not w:
            continue
        m = re.match(r"(.*)@(-?\d+),(-?\d+) (\d+)x(\d+)$", w)
        if m:
            out.append((m.group(1), int(m.group(4)), int(m.group(5))))
    return out


def norm_title(t):
    # version numbers and counters in titles differ between runs, not between libraries
    return re.sub(r"\d+", "#", t).strip().lower()


def match(wa, wb):
    """pairs of windows (a, b) by title, then the rest in order"""
    pairs, used = [], set()
    for a in wa:
        for j, b in enumerate(wb):
            if j not in used and norm_title(a[0]) == norm_title(b[0]):
                pairs.append((a, b)); used.add(j); break
        else:
            pairs.append((a, None))
    for j, b in enumerate(wb):
        if j not in used:
            pairs.append((None, b))
    return pairs


def lostnum(s):
    try:
        return int(s)
    except ValueError:
        return None


def differences(ra, rb, la, lb):
    d = []
    if ra is None or rb is None:
        return ["only in %s" % (lb if ra is None else la)]
    if ra["status"] != rb["status"]:
        d.append("status %s: %s, %s: %s" % (la, ra["status"], lb, rb["status"]))
    if ra["left"] != rb["left"]:
        d.append("still running after quit %s: %s, %s: %s" % (la, ra["left"], lb, rb["left"]))
    for a, b in match(ra["main"], rb["main"]):
        if a is None:
            d.append("window missing in %s: %r" % (la, b[0]))
        elif b is None:
            d.append("extra window in %s: %r" % (la, a[0]))
        elif (a[1], a[2]) != (b[1], b[2]):
            d.append("%r %dx%d in %s, %dx%d in %s (%+d, %+d)" % (a[0], a[1], a[2], la, b[1], b[2], lb, a[1] - b[1], a[2] - b[2]))
    xb = dict(rb["extras"])
    for name, wa in ra["extras"]:
        wb = xb.get(name, [])
        if len(wa) != len(wb):
            d.append("%s: %d window(s) in %s, %d in %s" % (name, len(wa), la, len(wb), lb))
        for a, b in match(wa, wb):
            if a and b and (a[1], a[2]) != (b[1], b[2]):
                d.append("%s %r %dx%d in %s, %dx%d in %s" % (name, a[0], a[1], a[2], la, b[1], b[2], lb))
    na, nb = lostnum(ra["lost"]), lostnum(rb["lost"])
    if na is not None and nb is not None and na > 1024 and na - nb > 1024:
        d.append("loses %d bytes per run in %s, %d in %s" % (na, la, nb, lb))
    elif (na is None) != (nb is None):
        d.append("memory: %s %s, %s %s" % (la, ra["lost"], lb, rb["lost"]))
    return d


def main():
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(2)
    A, B = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    la, lb = os.path.basename(A), os.path.basename(B)
    out = sys.argv[3] if len(sys.argv) > 3 else os.path.join(os.path.dirname(A), "compare-%s-%s.html" % (la, lb))
    ra, rb = load(A), load(B)
    ids = list(ra) + [i for i in rb if i not in ra]
    rel = lambda d, f: os.path.relpath(os.path.join(d, f), os.path.dirname(os.path.abspath(out)))
    h = ["<!doctype html><meta charset=utf-8><title>%s vs %s</title>" % (la, lb),
         "<style>body{font:14px sans-serif;background:#222;color:#ddd} table{border-collapse:collapse}"
         " td{vertical-align:top;padding:6px;border-top:1px solid #555} img{width:480px;image-rendering:pixelated}"
         " .bad{color:#f88} .ok{color:#8f8} code{color:#fd8}</style>",
         "<h1>Real-program suite: %s against %s</h1>" % (html.escape(la), html.escape(lb))]
    summary, same = [], 0
    rows = []
    for i in ids:
        a, b = ra.get(i), rb.get(i)
        d = differences(a, b, la, lb)
        if d:
            summary.append((i, d))
        else:
            same += 1
        cls = "bad" if d else "ok"
        info = lambda r, l: "<b>%s</b>: %s<br>%s<br>still running: %s, lost 2nd run: %s, in use: %s (1st run %s)" % (
            l, html.escape(r["status"]), "<br>".join("%s %dx%d" % (html.escape(w[0]), w[1], w[2]) for w in r["main"]) or "no window",
            r["left"], html.escape(r["lost"]), html.escape(r["inuse"]), html.escape(r["inuse1"])) if r else "<b>%s</b>: not run" % l
        rows.append("<tr><td colspan=2><h2 class=%s>%s</h2>%s</td></tr>" % (
            cls, html.escape(i), "<br>".join("<code>%s</code>" % html.escape(x) for x in d) or "same windows and sizes"))
        rows.append("<tr><td>%s</td><td>%s</td></tr>" % (info(a, la), info(b, lb)))
        shots = ["%s.png" % i]
        n = max(len(a["extras"]) if a else 0, len(b["extras"]) if b else 0)
        shots += ["%s-extra%d.png" % (i, k) for k in range(1, n + 1)]
        for s in shots:
            ia = "<img src='%s'>" % rel(A, s) if os.path.exists(os.path.join(A, s)) else "(no picture)"
            ib = "<img src='%s'>" % rel(B, s) if os.path.exists(os.path.join(B, s)) else "(no picture)"
            rows.append("<tr><td>%s</td><td>%s</td></tr>" % (ia, ib))
    h.append("<p>%d programs, %d the same in both, %d differ.</p>" % (len(ids), same, len(summary)))
    h.append("<table>" + "".join(rows) + "</table>")
    open(out, "w").write("\n".join(h))
    print("%d programs: %d behave the same in %s and %s, %d differ" % (len(ids), same, la, lb, len(summary)))
    for i, d in summary:
        print("  %s:" % i)
        for x in d:
            print("    " + x)
    # memory in use with the windows open, 2nd run and 1st run (from a fresh boot), bytes
    num = lambda r, k: int(r[k]) if r and r.get(k, "?").lstrip("-").isdigit() else None
    if any(num(ra.get(i), "inuse") is not None for i in ids):
        print("memory in use (2nd run / 1st run from boot), %s and %s:" % (la, lb))
        tot = [0, 0, 0, 0]
        for i in ids:
            v = [num(ra.get(i), "inuse"), num(rb.get(i), "inuse"),
                 num(ra.get(i), "inuse1"), num(rb.get(i), "inuse1")]
            print("  %-16s %10s %10s   %10s %10s" % ((i,) + tuple("?" if x is None else x for x in v)))
            for k in (0, 2):
                if v[k] is not None and v[k + 1] is not None:
                    tot[k] += v[k]; tot[k + 1] += v[k + 1]
        print("  %-16s %10d %10d   %10d %10d  (programs with both figures)" % (("total",) + tuple(tot)))
    print("contact sheet: %s" % out)


main()
