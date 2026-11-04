#!/usr/bin/env python3
# classsizes.py BUILDDIR: instance data size of every built-in class (from the _MUI_*_desc in each .o)
import sys, subprocess, glob, re, os
B = sys.argv[1]
CROSS = os.environ.get('CROSS', 'm68k-amigaos-'); nm = CROSS + 'nm'; od = CROSS + 'objdump'
res = {}
for o in sorted(glob.glob(B + '/classes/*.o')):
    syms = subprocess.run([nm, o], capture_output=True, text=True).stdout
    for m in re.finditer(r'^([0-9a-f]+) [TtDdRr] __MUI_(\w+)_desc$', syms, re.M):
        addr = int(m.group(1), 16)
        dump = subprocess.run([od, '-s', '-j', '.text', o], capture_output=True, text=True).stdout
        data = {}
        for l in dump.splitlines():
            p = l.split()
            if len(p) > 1 and re.match(r'^[0-9a-f]{4,}$', p[0]):
                base = int(p[0], 16)
                hexs = ''.join(t for t in p[1:5] if re.fullmatch(r'[0-9a-f]{2,8}', t))
                for i in range(0, len(hexs), 2): data[base + i // 2] = int(hexs[i:i+2], 16)
        v = int.from_bytes(bytes(data.get(addr + 8 + k, 0) for k in range(4)), 'big')
        res[m.group(2)] = v
for k, v in sorted(res.items()): print(f'{k:16s} {v}')
