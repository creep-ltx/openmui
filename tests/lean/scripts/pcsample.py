#!/usr/bin/env python3
"""pcsample.py SAMPLEFILE PROGRAM LIBRARY CLASSDIR [top]: report a tests/lean/pcsample.c run.

PROGRAM, LIBRARY: the unstripped files that were run (zunemaster/muimaster.library with
symbols); CLASSDIR: the build's Zune/ class files (cfNN in the sample file = the NNth
name in classfiles.h, which must sit next to LIBRARY). Prints a flat profile (where the
PC was) and an inclusive one (functions of the program/library/class files found on the
stack, once per sample)."""
import bisect, collections, os, re, subprocess, sys

def syms(path):
    out = subprocess.run(["m68k-amigaos-nm", "-n", path], capture_output=True, text=True).stdout
    s = []
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "tTwW":
            s.append((int(p[0], 16), p[2]))
    s.sort()
    return s

def main():
    f, prog, lib, cdir = sys.argv[1:5]
    top = int(sys.argv[5]) if len(sys.argv) > 5 else 40
    names = []
    cf = open(os.path.join(os.path.dirname(lib), "classfiles.h")).read()
    m = re.search(r"ZUNE_CLASSFILE_FILES\(X\)(.*)", cf)
    names = re.findall(r'X\("([^"]+)"\)', m.group(1))
    symcache = {}
    def symfile(seg):
        if seg == "program": return prog
        if seg in ("muimaster.library", "zunemaster.library"): return lib
        mcc = os.path.join(os.path.dirname(lib), "Libs", "MUI", seg)
        if os.path.exists(mcc): return mcc
        if seg.startswith("cf"): return os.path.join(cdir, names[int(seg[2:])])
        return None
    hunks = []  # (start, end, seg, hunk)
    libs = []
    samples = []
    for line in open(f, errors="replace"):
        p = line.split()
        if not p: continue
        if p[0] == "SEG":
            st = int(p[3], 16); hunks.append((st, st + int(p[4]), p[1], int(p[2])))
        elif p[0] == "LIB":
            libs.append((int(p[2], 16), int(p[3], 16), p[1]))
        elif p[0] == "S":
            # S pc tick stack...; MINTICK/MAXTICK keep a part of the run
            tick = int(p[2])
            if int(os.environ.get("MINTICK", 0)) <= tick <= int(os.environ.get("MAXTICK", 1 << 30)):
                samples.append([int(p[1], 16)] + [int(x, 16) for x in p[3:]])
        elif p[0] == "INFO":
            print(line.strip())
    hunks.sort()
    starts = [h[0] for h in hunks]
    def where(a, strict=False):
        i = bisect.bisect_right(starts, a) - 1
        if i >= 0 and a < hunks[i][1]:
            st, en, seg, hk = hunks[i]
            if hk != 0:
                return None if strict else "%s:hunk%d" % (seg, hk)
            path = symfile(seg)
            if path not in symcache: symcache[path] = syms(path) if path else []
            s = symcache[path]
            off = a - st
            j = bisect.bisect_right([x[0] for x in s], off) - 1
            nm = s[j][1] if j >= 0 else "?"
            label = seg if not seg.startswith("cf") else names[int(seg[2:])]
            return "%s:%s" % (label if seg != "zunemaster.library" else "lib", nm)
        if strict: return None
        if 0xF80000 <= a < 0x1000000: return "ROM:%05x" % ((a - 0xF80000) & ~0xff)
        best = None
        for lo, hi, nm in libs:
            if lo - 0x200 <= a <= hi + 0x4000:
                best = nm
        return "LIB:%s" % best if best else "?:%08x" % a
    flat = collections.Counter()
    incl = collections.Counter()
    for s in samples:
        flat[where(s[0])] += 1
        seen = set()
        w = where(s[0])
        seen.add(w)
        for a in s[1:]:
            if a & 1: continue
            x = where(a, strict=True)
            if x: seen.add(x)
        for x in seen: incl[x] += 1
    n = len(samples) or 1
    pat = os.environ.get("CHAINS")
    if pat:
        # CHAINS=label-substring: the first four known callers of those samples
        chains = collections.Counter()
        for s in samples:
            if pat not in where(s[0]): continue
            ch = []
            for a in s[1:]:
                if a & 1: continue
                x = where(a, strict=True)
                if x and (not ch or ch[-1] != x): ch.append(x)
                if len(ch) >= 4: break
            chains[" < ".join(ch)] += 1
        print("chains of %s:" % pat)
        for k, v in chains.most_common(top): print("%5d %s" % (v, k))
        return
    print("flat (%d samples):" % len(samples))
    for k, v in flat.most_common(top): print("%5d %5.1f%% %s" % (v, 100.0 * v / n, k))
    print("inclusive:")
    for k, v in incl.most_common(top): print("%5d %5.1f%% %s" % (v, 100.0 * v / n, k))

main()
