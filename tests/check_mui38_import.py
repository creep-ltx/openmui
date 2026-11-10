#!/usr/bin/env python3
"""MUI 3.8 settings files read by Zune prefs (workbench/prefs/Zune/mui38prefs.c).

The converter is plain C: it is compiled for the PC and run on a file made
here the way MUI 3.8's settings program writes it (FORM PREF, PRHD, MUIC
items without padding, MUIW window positions; ids measured black-box, see
docs/lean/README.md "Reading MUI 3.8's settings files").
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'workbench/prefs/Zune'


def chunk(cid, data):
    return cid + struct.pack('>I', len(data)) + data + (b'\0' if len(data) & 1 else b'')


def item(iid, value):
    if isinstance(value, int):
        value = struct.pack('>I', value)
    elif isinstance(value, str):
        value = value.encode('latin1') + b'\0'
    return struct.pack('>II', iid, len(value)) + value


def form(*chunks):
    body = b'PREF' + b''.join(chunks)
    return b'FORM' + struct.pack('>I', len(body)) + body


def items(data):
    assert data[:4] == b'FORM' and data[8:12] == b'PREF'
    assert struct.unpack('>I', data[4:8])[0] == len(data) - 8
    out, p = {}, 12
    while p + 8 <= len(data):
        cid, n = data[p:p + 4], struct.unpack('>I', data[p + 4:p + 8])[0]
        if cid == b'MUIC':
            q, end = p + 8, p + 8 + n
            while q + 8 <= end:
                iid, ln = struct.unpack('>II', data[q:q + 8])
                assert iid not in out, hex(iid)
                out[iid] = data[q + 8:q + 8 + ln]
                q += 8 + ln
            assert q == end
        p += 8 + n + (n & 1)
    return out


def main():
    cc = shutil.which('cc') or shutil.which('gcc')
    if not cc:
        print('check_mui38_import: no host C compiler, skipped')
        return
    tmp = Path(tempfile.mkdtemp(prefix='mui38imp'))
    try:
        exe = tmp / 'conv'
        subprocess.run([cc, '-O1', '-Wall', '-Werror', '-I', str(SRC), '-o', str(exe),
                        str(ROOT / 'tests/native/mui38conv.c'),
                        str(SRC / 'mui38prefs.c')], check=True)
        muiw = (b'MAIN' + struct.pack('>I', 40)
                + struct.pack('>20h', 70, 30, 500, 196, 70, 30, 500, 196,
                              *([0] * 10), 1, 0)
                + b'SUBW' + struct.pack('>I', 40)
                + struct.pack('>20h', 10, 20, 300, 100, *([0] * 16)))
        src = form(chunk(b'PRHD', bytes([0x13, 0, 0, 0, 0, 0])),
                   chunk(b'MUIC', b''.join([
                       item(0x24, 'TESTSCR'),          # public screen: same id
                       item(0x29, 'ctrl alt z'),       # iconify hotkey
                       item(0x7c, 0), item(0x7d, 1),   # pop to front, show menu
                       item(0x14, 0), item(0x15, 1),   # show icon, on startup
                       item(0x7e, 0),                  # ARexx
                       item(0x81, 22), item(0x82, 4),  # bubbles, tenths of s
                       item(0x0e, 2), item(0x1c, 1),   # frame thickness, refresh
                       item(0x13, 1), item(0x76, 0), item(0x77, 1), item(0x78, 1),
                       item(0x2b, '602211'), item(0x33, '916644'),
                       item(0x34, '112211'), item(0x90, 'a02200'),
                       item(0x46, '3:XEN/CheckMark.mf0'),
                       item(0x4c, '3:XEN/xenbar.image'),
                       item(0x37, '2:m2'), item(0x54, '0:139'),
                       item(0x8b, 'alt'),
                       item(0x80020001, b'2:p4' + b'\0' * 60),  # another class
                   ])),
                   chunk(b'MUIW', muiw))
        (tmp / 'in.prefs').write_bytes(src)
        subprocess.run([str(exe), str(tmp / 'in.prefs'), str(tmp / 'out.prefs')], check=True)
        got = items((tmp / 'out.prefs').read_bytes())
        u = lambda i: struct.unpack('>I', got[i])[0]
        s = lambda i: got[i].rstrip(b'\0').decode('latin1')
        assert s(0x24) == 'TESTSCR'
        assert s(0x701) == 'ctrl alt z'
        assert (u(0x700), u(0x703), u(0x702), u(0x704), u(0x705)) == (0, 1, 0, 1, 0)
        assert (u(0x706), u(0x707)) == (22, 4)
        assert (u(0x50b), u(0x404)) == (2, 1)
        assert u(0x50a) == 4 | 1 | 8   # iconify, prefs (MUI), popup; no snapshot
        for gone in (0x0e, 0x1c, 0x13, 0x14, 0x15, 0x29, 0x76, 0x77, 0x78,
                     0x7c, 0x7d, 0x7e, 0x81, 0x82):
            assert gone not in got, hex(gone)
        assert (s(0x2b), s(0x33), s(0x34), s(0x90)) == ('902211', '716644', '112211', '502200')
        assert s(0x46) == '4:XEN/CheckMark.mf0'   # a brush
        assert s(0x4c) == '3:XEN/xenbar.image'    # a BOOPSI image class
        assert (s(0x37), s(0x54), s(0x8b)) == ('2:m2', '0:139', 'alt')
        assert got[0x80020001] == b'2:p4' + b'\0' * 60
        size, = struct.unpack('>I', got[0x509][:4])
        assert size == 4 + 2 * 20 == len(got[0x509])
        assert got[0x509][4:24] == b'MAIN' + struct.pack('>8h', 70, 30, 500, 196, 70, 30, 500, 196)
        assert got[0x509][24:44] == b'SUBW' + struct.pack('>8h', 10, 20, 300, 100, 10, 20, 300, 100)
        # pictures that cannot be found are left out (Zune's default stays)
        subprocess.run([str(exe), str(tmp / 'in.prefs'), str(tmp / 'out2.prefs'), 'nofiles'],
                       check=True)
        got2 = items((tmp / 'out2.prefs').read_bytes())
        assert 0x46 not in got2 and 0x4c not in got2 and 0x37 in got2
        # not a settings file
        (tmp / 'bad').write_bytes(b'FORM\0\0\0\4ILBM')
        assert subprocess.run([str(exe), str(tmp / 'bad'), str(tmp / 'o')]).returncode == 1
        # a file MUI 3.8 wrote with nothing changed
        (tmp / 'empty').write_bytes(form(chunk(b'PRHD', bytes(6)), chunk(b'MUIC', b'')))
        subprocess.run([str(exe), str(tmp / 'empty'), str(tmp / 'o')], check=True)
        assert items((tmp / 'o').read_bytes()) == {}
    finally:
        shutil.rmtree(tmp)
    print('check_mui38_import: ok')


if __name__ == '__main__':
    main()
