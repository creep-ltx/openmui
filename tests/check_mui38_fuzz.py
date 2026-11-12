#!/usr/bin/env python3
"""Fuzz the MUI 3.8 settings-file importer on the PC under the address sanitizer (gate 2).

workbench/prefs/Zune/mui38prefs.c (the converter Zune prefs runs on ENV:MUI files) is plain C:
it is built for the host with -fsanitize=address,undefined through tests/native/mui38conv.c and
given mutations of a valid MUI 3.8 file: flipped bytes, truncations, chunk and item lengths set
to huge, odd, zero or negative values, items without a terminator, duplicated and shuffled
chunks, and random files that start like one. The converter may reject a file (exit 1) but
must never read or write outside its buffers (the sanitizer's report is the failure).
FUZZ_CASES (default 1500) and FUZZ_SEED (default 1) in the environment. Skipped without cc.
"""
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'workbench/prefs/Zune'
CASES = int(os.environ.get('FUZZ_CASES', '1500'))
SEED = int(os.environ.get('FUZZ_SEED', '1'))


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


def valid():
    muiw = (b'MAIN' + struct.pack('>I', 40)
            + struct.pack('>20h', 70, 30, 500, 196, 70, 30, 500, 196, *([0] * 10), 1, 0)
            + b'SUBW' + struct.pack('>I', 40)
            + struct.pack('>20h', 10, 20, 300, 100, *([0] * 16)))
    return form(chunk(b'PRHD', bytes([0x13, 0, 0, 0, 0, 0])),
                chunk(b'MUIC', b''.join([
                    item(0x24, 'TESTSCR'), item(0x29, 'ctrl alt z'),
                    item(0x7c, 0), item(0x7d, 1), item(0x14, 0), item(0x15, 1), item(0x7e, 0),
                    item(0x81, 22), item(0x82, 4), item(0x0e, 2), item(0x1c, 1),
                    item(0x13, 1), item(0x76, 0), item(0x77, 1), item(0x78, 1),
                    item(0x2b, '602211'), item(0x33, '916644'), item(0x34, '112211'),
                    item(0x90, 'a02200'), item(0x46, '3:XEN/CheckMark.mf0'),
                    item(0x4c, '3:XEN/xenbar.image'), item(0x37, '2:m2'), item(0x54, '0:139'),
                    item(0x8b, 'alt'), item(0x80020001, b'2:p4' + b'\0' * 60),
                ])),
                chunk(b'MUIW', muiw))


def mutate(rnd, src):
    data = bytearray(src)
    kind = rnd.randrange(9)
    if len(data) < 24 and kind not in (1, 5):
        kind = 5
    if kind == 0:                         # flipped bytes
        for _ in range(rnd.randint(1, 8)):
            i = rnd.randrange(len(data)); data[i] = rnd.randrange(256)
    elif kind == 1:                       # truncated
        data = data[:rnd.randrange(len(data) + 1)]
    elif kind == 2:                       # a length field (chunk or item) rewritten
        pos = [i for i in range(4, len(data) - 3, 2)]
        for _ in range(rnd.randint(1, 3)):
            i = rnd.choice(pos)
            v = rnd.choice([0, 1, 2, 3, 0x7fffffff, 0x80000000, 0xffffffff, 0xfffffffe, len(data), len(data) * 2, rnd.randrange(0x10000)])
            data[i:i + 4] = struct.pack('>I', v)
    elif kind == 3:                       # a string item without its terminator
        i = data.find(b'TESTSCR\0')
        if i >= 0:
            data[i + 7] = ord('X')
    elif kind == 4:                       # chunks duplicated or shuffled
        extra = rnd.choice([chunk(b'MUIC', bytes(rnd.randrange(256) for _ in range(rnd.randrange(40)))),
                            chunk(b'MUIW', bytes(rnd.randrange(256) for _ in range(rnd.randrange(90)))),
                            chunk(b'PRHD', b''), chunk(b'JUNK', b'x' * rnd.randrange(50))])
        i = rnd.randrange(12, len(data))
        data[i:i] = extra
        data[4:8] = struct.pack('>I', len(data) - 8)
    elif kind == 5:                       # random bytes after a right start
        n = rnd.randrange(0, 400)
        data = bytearray(b'FORM' + struct.pack('>I', n + 4) + b'PREF') + bytearray(rnd.randrange(256) for _ in range(n))
    elif kind == 6:                       # an item's size odd or far too small/large for its id
        i = data.find(b'MUIC')
        if i >= 0 and i + 16 < len(data):
            j = i + 8 + 8 * rnd.randrange(0, 3)
            data[j + 4:j + 8] = struct.pack('>I', rnd.choice([0, 1, 3, 5, 7, 255, 4000, 0x7ffffff0]))
    elif kind == 7:                       # a FORM length that lies
        data[4:8] = struct.pack('>I', rnd.choice([0, 4, 8, 0xffffffff, 0x7fffffff, len(data) + 100]))
    else:                                 # several of the above
        for _ in range(rnd.randint(2, 4)):
            data = bytearray(mutate(rnd, bytes(data)))
    return bytes(data)


def main():
    cc = shutil.which('cc') or shutil.which('gcc')
    if not cc:
        print('check_mui38_fuzz: no host C compiler, skipped')
        return
    rnd = random.Random(SEED)
    tmp = Path(tempfile.mkdtemp(prefix='mui38fuzz'))
    try:
        exe = tmp / 'conv'
        r = subprocess.run([cc, '-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                            '-I', str(SRC), '-o', str(exe), str(ROOT / 'tests/native/mui38conv.c'),
                            str(SRC / 'mui38prefs.c')], capture_output=True, text=True)
        if r.returncode:
            print('check_mui38_fuzz: the sanitizer build failed, skipped:', r.stderr.strip()[:200])
            return
        src = valid()
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:allocator_may_return_null=1', UBSAN_OPTIONS='print_stacktrace=1')
        fails = 0
        for i in range(CASES):
            data = mutate(rnd, src)
            (tmp / 'in.prefs').write_bytes(data)
            for args in ([], ['nofiles']):
                r = subprocess.run([str(exe), str(tmp / 'in.prefs'), str(tmp / 'out.prefs')] + args,
                                   capture_output=True, text=True, env=env, timeout=20)
                if r.returncode not in (0, 1, 2) or 'Sanitizer' in r.stderr or 'runtime error' in r.stderr:
                    fails += 1
                    keep = tmp / f'fail{fails}.prefs'
                    keep.write_bytes(data)
                    print(f'case {i} seed {SEED} args {args}: exit {r.returncode}, input kept as {keep}')
                    print(r.stderr.strip()[:1500])
                    if fails > 3:
                        break
            if fails > 3:
                break
        assert not fails, f'{fails} importer failures'
        print(f'check_mui38_fuzz: {CASES} mutated MUI 3.8 settings files passed under the sanitizer (seed {SEED})')
    finally:
        if not any(tmp.glob('fail*.prefs')):
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    main()
