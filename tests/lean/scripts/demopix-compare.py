#!/usr/bin/env python3
"""demopix-compare.py OUT: compare the demo shots of demopix.sh pixel by pixel.

OUT/zune and OUT/mui38 hold, per shot, SHOT.png (the whole screen, unscaled) and SHOT.wins
(wasabi windows --raw lines of the shot's windows). Each window is cropped from its screen,
matched with the other side's window of the same title (in order when titles repeat) and
compared pixel by pixel. Windows of different sizes are compared on the larger box, the missing
part counting as different. Writes OUT/report.html (both windows side by side, the difference
picture: differing pixels red, equal ones dimmed), OUT/cmp/*.png and OUT/results.tsv (shot,
title, Zune size, MUI 3.8 size, differing pixels, pixels), and prints a summary.
"""
import html
import os
import sys

from PIL import Image

SCALE_Y = 2  # the pictures in the report get their rows doubled (640x256 is a 1:2 pixel)

# shots that cannot be the same twice, on any library
NOT_COMPARABLE = {
    "Layout": "places its buttons at random",
    "InputHandler": "its text changes every second",
}


def load_wins(path):
    wins = []
    if not os.path.exists(path):
        return None
    for line in open(path, encoding="latin-1"):
        f = line.rstrip("\n").split("\t")
        if len(f) < 8 or f[0] != "W":
            continue
        x, y, w, h = (int(v) for v in f[2:6])
        wins.append(dict(title=f[-1], x=x, y=y, w=w, h=h))
    return wins


def pairs(za, mb):
    """windows matched by title, repeated titles in order; the rest unmatched"""
    out, used = [], set()
    for a in za:
        for j, b in enumerate(mb):
            if j not in used and a["title"] == b["title"]:
                out.append((a, b)); used.add(j); break
        else:
            out.append((a, None))
    out += [(None, b) for j, b in enumerate(mb) if j not in used]
    return out


def crop(img, w):
    return img.crop((w["x"], w["y"], w["x"] + w["w"], w["y"] + w["h"]))


def compare(a, b):
    """count of differing pixels on the union box, and the difference picture"""
    W, H = max(a.width, b.width), max(a.height, b.height)
    pa, pb = a.load(), b.load()
    diff = Image.new("RGB", (W, H))
    pd = diff.load()
    n = 0
    for y in range(H):
        for x in range(W):
            ina = x < a.width and y < a.height
            inb = x < b.width and y < b.height
            if ina and inb and pa[x, y] == pb[x, y]:
                r, g, bb = pa[x, y]
                pd[x, y] = (r // 3 + 120, g // 3 + 120, bb // 3 + 120)
            else:
                n += 1
                pd[x, y] = (255, 0, 0) if ina and inb else (255, 0, 255)
    return n, W * H, diff


def big(img):
    return img.resize((img.width, img.height * SCALE_Y), Image.NEAREST)


def main():
    out = sys.argv[1]
    zd, md = os.path.join(out, "zune"), os.path.join(out, "mui38")
    cd = os.path.join(out, "cmp")
    os.makedirs(cd, exist_ok=True)
    shots = sorted({f[:-5] for d in (zd, md) for f in os.listdir(d) if f.endswith(".wins")},
                   key=lambda s: (s.startswith("MUIDemo-"), s.lower()))
    rows, tsv = [], []
    for s in shots:
        zw, mw = load_wins(os.path.join(zd, s + ".wins")), load_wins(os.path.join(md, s + ".wins"))
        zi = mi = None
        if os.path.exists(os.path.join(zd, s + ".png")):
            zi = Image.open(os.path.join(zd, s + ".png")).convert("RGB")
        if os.path.exists(os.path.join(md, s + ".png")):
            mi = Image.open(os.path.join(md, s + ".png")).convert("RGB")
        for k, (a, b) in enumerate(pairs(zw or [], mw or [])):
            title = (a or b)["title"]
            base = "%s-%d" % (s, k)
            r = dict(shot=s, title=title, z=None, m=None, n=None, tot=None, base=base)
            ca = crop(zi, a) if a and zi else None
            cb = crop(mi, b) if b and mi else None
            if ca:
                big(ca).save(os.path.join(cd, base + "-zune.png")); r["z"] = "%dx%d" % (a["w"], a["h"])
            if cb:
                big(cb).save(os.path.join(cd, base + "-mui38.png")); r["m"] = "%dx%d" % (b["w"], b["h"])
            if ca and cb:
                r["n"], r["tot"], diff = compare(ca, cb)
                big(diff).save(os.path.join(cd, base + "-diff.png"))
            rows.append(r)
            tsv.append("\t".join(str(v) for v in (s, title, r["z"] or "-", r["m"] or "-",
                                                   "-" if r["n"] is None else r["n"], r["tot"] or "-")))
    with open(os.path.join(out, "results.tsv"), "w") as f:
        f.write("\n".join(tsv) + "\n")
    same = [r for r in rows if r["n"] == 0]
    comparable = [r for r in rows if r["shot"] not in NOT_COMPARABLE]
    with open(os.path.join(out, "report.html"), "w", encoding="utf-8") as f:
        f.write("<!doctype html><meta charset=utf-8><title>Zune against MUI 3.8: the demo set</title>"
                "<style>body{font-family:sans-serif;background:#333;color:#eee}"
                "td{vertical-align:top;padding:4px}img{image-rendering:pixelated}"
                ".ok{color:#7f7}.bad{color:#f77}</style>")
        f.write("<h1>Zune against MUI 3.8: the demo set, pixel by pixel</h1>")
        f.write("<p>%d windows, %d pixel-identical (%d can be compared: %s). Rows doubled for "
                "viewing. In the difference picture red = a different pixel, magenta = only one side "
                "has the window there.</p>"
                % (len(rows), len(same), len(comparable),
                   "; ".join("%s %s" % kv for kv in NOT_COMPARABLE.items())))
        f.write("<table><tr><th>shot / window</th><th>sizes</th><th>differing</th></tr>")
        for r in rows:
            cls = "ok" if r["n"] == 0 else "bad"
            if r["shot"] in NOT_COMPARABLE:
                cls = ""
            pct = "" if not r["tot"] else " (%.1f%%)" % (100.0 * r["n"] / r["tot"])
            f.write("<tr><td><a href='#%s'>%s / %s</a></td><td>%s / %s</td><td class=%s>%s%s</td></tr>"
                    % (r["base"], html.escape(r["shot"]), html.escape(r["title"]), r["z"] or "-",
                       r["m"] or "-", cls, "-" if r["n"] is None else r["n"], pct))
        f.write("</table>")
        for r in rows:
            f.write("<h2 id='%s'>%s / %s: %s</h2><table><tr><th>Zune %s</th><th>MUI 3.8 %s</th>"
                    "<th>difference</th></tr><tr>" % (r["base"], html.escape(r["shot"]),
                    html.escape(r["title"]), "-" if r["n"] is None else "%d pixels differ" % r["n"],
                    r["z"] or "missing", r["m"] or "missing"))
            for side in ("zune", "mui38", "diff"):
                p = os.path.join("cmp", "%s-%s.png" % (r["base"], side))
                f.write("<td>%s</td>" % ("<img src='%s'>" % p if os.path.exists(os.path.join(out, p)) else "-"))
            f.write("</tr></table>")
    print("%d windows, %d pixel-identical, %d comparable (not: %s)"
          % (len(rows), len(same), len(comparable), ", ".join(NOT_COMPARABLE)))
    for r in rows:
        print("%-22s %-28s %-9s %-9s %s" % (r["shot"], r["title"][:28], r["z"] or "-", r["m"] or "-",
                                         "-" if r["n"] is None else r["n"]))
    print("report: " + os.path.join(out, "report.html"))


if __name__ == "__main__":
    main()
