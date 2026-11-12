#!/usr/bin/env python3
# prefsmutate.py SEED OUT [--base OUT]: a mutated Zune settings file (FORM PREF, PRHD, MUIC) for
# the prefs fuzz (tests/lean/scripts/prefsfuzz.sh). The base file holds every setting the library
# reads, with its default value, taken from the sources (classes/configdata.h for the ids,
# configdata.c's default tables for the values): fonts, pen, image and frame specs, key strings,
# ULONGs, custom frames and images. SEED 0 writes the base unchanged; another seed applies one
# or more of: a string item without its terminator, an item length that lies (0, 1, odd, huge),
# a ULONG item of the wrong size, random bytes in a spec string, a spec string of 300 characters,
# a ULONG with an extreme value, a truncated file, a chunk length that lies, duplicated items,
# an unknown id, flipped bytes. --base OUT writes the base file too.
import random
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
MM = ROOT / 'workbench/libs/muimaster'


def ids():
    out = {}
    for m in re.finditer(r'#define\s+(MUICFG_\w+)\s+(0x[0-9a-fA-F]+|\d+)', (MM / 'classes/configdata.h').read_text()):
        out[m.group(1)] = int(m.group(2), 0)
    return out


def constants():
    """#define NAME number in the library's headers, for the symbolic defaults"""
    out = {'TRUE': 1, 'FALSE': 0}
    for h in list(MM.glob('*.h')) + list(MM.glob('classes/*.h')):
        for m in re.finditer(r'#define\s+([A-Z_][A-Z0-9_]*)\s+\(?(-?0x[0-9a-fA-F]+|-?\d+)\)?', h.read_text(errors='replace')):
            out.setdefault(m.group(1), int(m.group(2), 0))
    return out


def defaults(cfg):
    src = (MM / 'classes/configdata.c').read_text()
    const = constants()
    items = []
    # ULONG settings: PREF_DEFAULT(MUICFG_x, value, field); the symbolic values from the headers.
    # "Iconify at startup" is left out: on, the program opens no window by design.
    for m in re.finditer(r'PREF_DEFAULT\((MUICFG_\w+),\s*([^,]+),', src):
        if m.group(1) in cfg and m.group(1) != 'MUICFG_Iconification_OnStartup':
            v = m.group(2).strip()
            items.append((cfg[m.group(1)], int(v, 0) if re.match(r'-?(0x)?[0-9a-fA-F]+$', v) else const.get(v, 0)))
    # string settings: {MUICFG_x, FIELD(...), "value"}
    for m in re.finditer(r'\{(MUICFG_\w+),\s*FIELD\([^)]*\)(?:\s*\|\s*KEYFIELD)?,\s*"([^"]*)"\}', src):
        if m.group(1) in cfg:
            items.append((cfg[m.group(1)], m.group(2) or 'topaz.font/8'))
    # image and frame specs: { MUII_x, MUICFG_y, "spec" }
    for m in re.finditer(r'\{\s*MUI\w+,\s*(MUICFG_\w+),\s*"([^"]*)"\s*\}', src):
        if m.group(1) in cfg and cfg[m.group(1)]:
            items.append((cfg[m.group(1)], m.group(2)))
    # custom frames and images, a public screen, a hotkey
    for i in range(16):
        if 'MUICFG_CustomFrame_1' in cfg:
            items.append((cfg['MUICFG_CustomFrame_1'] + i, '212211'))
    for name, v in (('MUICFG_PublicScreen', 'Workbench'), ('MUICFG_Iconification_Hotkey', 'control alt z')):
        if name in cfg:
            items.append((cfg[name], v))
    return items


def item(iid, value):
    if isinstance(value, int):
        value = struct.pack('>I', value & 0xffffffff)
    elif isinstance(value, str):
        value = value.encode('latin1') + b'\0'
    return struct.pack('>II', iid, len(value)) + value


def build(items):
    muic = b''.join(item(i, v) for i, v in items)
    prhd = b'PRHD' + struct.pack('>I', 6) + bytes(6)
    muic = b'MUIC' + struct.pack('>I', len(muic)) + muic + (b'\0' if len(muic) & 1 else b'')
    body = b'PREF' + prhd + muic
    return b'FORM' + struct.pack('>I', len(body)) + body


SPECCHARS = b'0123456789abcdefmsrp:,-HV/. '


def mutate(rnd, items):
    items = list(items)
    data = None
    kind = rnd.randrange(12)
    strings = [k for k, (i, v) in enumerate(items) if isinstance(v, str)]
    ulongs = [k for k, (i, v) in enumerate(items) if isinstance(v, int)]
    if kind == 0:      # a string without its terminator
        k = rnd.choice(strings); i, v = items[k]
        items[k] = (i, v.encode('latin1'))
    elif kind == 1:    # random bytes in a spec string
        k = rnd.choice(strings); i, v = items[k]
        b = bytearray(v.encode('latin1'))
        for _ in range(rnd.randint(1, 4)):
            if b: b[rnd.randrange(len(b))] = rnd.choice(SPECCHARS) if rnd.random() < 0.7 else rnd.randrange(1, 256)
        items[k] = (i, bytes(b) + b'\0')
    elif kind == 2:    # a very long spec string
        k = rnd.choice(strings); i, v = items[k]
        items[k] = (i, bytes(rnd.choice(SPECCHARS) for _ in range(rnd.choice([40, 100, 300, 1000]))) + b'\0')
    elif kind == 3:    # a ULONG of the wrong size or an extreme value
        k = rnd.choice(ulongs); i, v = items[k]
        items[k] = (i, rnd.choice([b'', b'\1', b'\0\1', b'\0\0\1', struct.pack('>I', rnd.choice([0, 0xffffffff, 0x7fffffff, 0x80000000, 65536, 1000000]))]))
    elif kind == 4:    # a string where a ULONG is read and the other way round
        k = rnd.choice(ulongs); i, v = items[k]; items[k] = (i, '2:m2')
        k = rnd.choice(strings); i, v = items[k]; items[k] = (i, rnd.choice([0, 7, 0xffffffff]))
    elif kind == 5:    # duplicated items and an unknown id
        for _ in range(rnd.randint(1, 5)):
            items.append(rnd.choice(items))
        items.append((rnd.choice([0, 0x7fff, 0xffffffff, 0x80000001, 0x1234]), rnd.choice(['x', 5, b'\xff' * 9])))
    elif kind == 6:    # an empty string, a spec of one character
        k = rnd.choice(strings); i, v = items[k]; items[k] = (i, rnd.choice(['', 'r', '2', '5:', '3:', 'H', '-']))
    data = bytearray(build(items))
    if kind == 7:      # an item length that lies
        p = 12 + 8 + 6 + 8
        for _ in range(rnd.randint(1, 3)):
            q = p + 4
            # walk to a random item
            n = rnd.randrange(len(items)); pos = p
            for k in range(n):
                if pos + 8 > len(data): break
                pos += 8 + struct.unpack('>I', data[pos + 4:pos + 8])[0]
            if pos + 8 <= len(data):
                data[pos + 4:pos + 8] = struct.pack('>I', rnd.choice([0, 1, 3, 0x7fffffff, 0xffffffff, 0xfffffffc, len(data), 5000]))
    elif kind == 8:    # truncated
        data = data[:rnd.randrange(12, len(data))]
    elif kind == 9:    # chunk lengths that lie
        off = rnd.choice([4, 16, 30])
        if off + 4 <= len(data):
            data[off:off + 4] = struct.pack('>I', rnd.choice([0, 1, 7, 0x7fffffff, 0xffffffff, len(data) * 3]))
    elif kind == 10:   # flipped bytes
        for _ in range(rnd.randint(1, 12)):
            data[rnd.randrange(len(data))] = rnd.randrange(256)
    elif kind == 11:   # odd item sizes everywhere (no padding in MUIC: 2 bytes per item lost)
        data = bytearray(build([(i, (v.encode('latin1') if isinstance(v, str) else struct.pack('>I', v))[:-1] if rnd.random() < 0.5 else v) for i, v in items]))
    return bytes(data)


def main():
    seed = int(sys.argv[1]); out = sys.argv[2]
    base = sys.argv[4] if len(sys.argv) > 4 and sys.argv[3] == '--base' else None
    items = defaults(ids())
    if base:
        Path(base).write_bytes(build(items))
    rnd = random.Random(seed)
    data = build(items) if seed == 0 else mutate(rnd, items)
    if seed and rnd.random() < 0.3:
        data = mutate(random.Random(seed * 7919), items)
    Path(out).write_bytes(data)
    print(f'{out}: seed {seed}, {len(items)} settings, {len(data)} bytes')


if __name__ == '__main__':
    main()
