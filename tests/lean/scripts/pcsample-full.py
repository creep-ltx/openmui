#!/usr/bin/env python3
"""pcsample-full.py SAMPLES PROGRAM LIBDIR [top]: pcsample.py's report with the static functions.

The library's hunk symbols hold its global functions only, so pcsample.py counts a static
function under the global before it. Here muimaster.library's symbols come from the link map
(LIBDIR/muimaster.library.map: the .text address of each object) and each object's own local
symbols (nm -n): every function by its name. LIBDIR is a build folder (with Zune/ and
classfiles.h, as pcsample.py wants). CHAINS=label works as there."""
import os, re, subprocess, sys, types

here = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(here, 'pcsample.py')).read().replace('\nmain()', '\npass')
pcs = types.ModuleType('pcs')
exec(compile(src, 'pcsample.py', 'exec'), pcs.__dict__)
NM = 'm68k-amigaos-nm'
orig = pcs.syms

def full(path):
    mp = path + '.map'
    if not os.path.exists(mp):
        return orig(path)
    s = []
    for line in open(mp):
        m = re.match(r'\s*\.text\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+\.o)$', line)
        if not m or not os.path.exists(m.group(3)):
            continue
        base = int(m.group(1), 16)
        out = subprocess.run([NM, '-n', m.group(3)], capture_output=True, text=True).stdout
        for l in out.splitlines():
            p = l.split()
            if len(p) == 3 and p[1] in 'tT':
                s.append((base + int(p[0], 16), p[2]))
    s.sort()
    return s or orig(path)

pcs.__dict__['syms'] = full
lib = os.path.join(sys.argv[3], 'muimaster.library')
sys.argv = [sys.argv[0], sys.argv[1], sys.argv[2], lib, os.path.join(sys.argv[3], 'Zune')] + sys.argv[4:]
pcs.main()
