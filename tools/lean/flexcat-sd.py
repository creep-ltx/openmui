#!/usr/bin/env python3
"""Minimal FlexCat stand-in: source descriptors (.sd) from a .cd or .pot file.

Supports what Zune68's build uses: `flexcat IN.cd|IN.pot OUT=TEMPLATE.sd [...]`.
Codes: %b %i %d %s %e %a %t %z %l %v %f0 %fv %%, with an optional byte width (%4a, %2t).
No catalogs (CATALOG): those are only read for non-English locales.
"""
import re
import sys
from pathlib import Path


def cd_unescape(s):
    out = bytearray()
    i = 0
    simple = {'n': 10, 't': 9, 'r': 13, 'b': 8, 'f': 12, 'v': 11, 'a': 7, 'g': 7,
              'e': 27, 'c': 0x9b, '\\': 92, '"': 34, "'": 39, '0': 0}
    b = s.encode('latin-1', 'replace')
    while i < len(b):
        c = b[i]
        if c == 92 and i + 1 < len(b):
            n = chr(b[i + 1])
            if n == 'x':
                m = re.match(rb'[0-9a-fA-F]{1,2}', b[i + 2:])
                out.append(int(m.group(0), 16)); i += 2 + len(m.group(0)); continue
            if n in '01234567':
                m = re.match(rb'[0-7]{1,3}', b[i + 1:])
                out.append(int(m.group(0), 8) & 255); i += 1 + len(m.group(0)); continue
            if n in simple:
                out.append(simple[n]); i += 2; continue
            out.append(b[i + 1]); i += 2; continue
        out.append(c); i += 1
    return bytes(out)


def parse_cd(path):
    text = Path(path).read_bytes().decode('latin-1')
    lines = text.split('\n')
    msgs, nextid, i = [], 0, 0
    while i < len(lines):
        line = lines[i].rstrip('\r')
        i += 1
        if not line or line.startswith(';') or line.startswith('#'):
            continue
        m = re.match(r'^(\w+)\s*\(([^)]*)\)', line)
        if not m:
            continue
        name, spec = m.group(1), m.group(2).split('/')[0].strip()
        if spec.startswith('+'):
            nextid += int(spec[1:])
        elif spec:
            nextid = int(spec, 0)
        s = ''
        while i < len(lines):
            l = lines[i].rstrip('\r'); i += 1
            if l.endswith('\\') and not l.endswith('\\\\'):
                s += l[:-1]; continue
            s += l; break
        msgs.append((name, nextid, cd_unescape(s)))
        nextid += 1
    return msgs


def parse_pot(path):
    text = Path(path).read_text('utf-8')
    msgs, nextid = [], 0
    for m in re.finditer(r'msgctxt\s+"([^"]*)"\s*\nmsgid\s+((?:"(?:[^"\\]|\\.)*"\s*)+)', text):
        cm = re.match(r'(\w+)\s*\(([^)]*)\)', m.group(1))
        name, spec = cm.group(1), cm.group(2).split('/')[0].strip()
        if spec:
            nextid = int(spec, 0)
        s = ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(2)))
        raw = cd_unescape(s.encode('utf-8').decode('latin-1'))
        msgs.append((name, nextid, raw))
        nextid += 1
    return msgs


def c_string(b):
    out = '"'
    for c in b:
        if c == 34: out += '\\"'
        elif c == 92: out += '\\\\'
        elif c == 10: out += '\\n'
        elif c == 9: out += '\\t'
        elif 32 <= c < 127: out += chr(c)
        else: out += '\\%03o' % c
    return out + '"'


def esc_bytes(n, width):
    return ''.join('\\x%02X' % ((n >> (8 * k)) & 255) for k in reversed(range(width)))


CODE = re.compile(r'%(\d*)([bidseatzlvf%])(0|v)?')


def subst(line, msg, base, src, perline):
    def rep(m):
        width, c, extra = int(m.group(1) or 4), m.group(2), m.group(3)
        if c == '%': return '%'
        if c == 'b': return base
        if c == 'l': return '"english"'
        if c == 'v': return '0'
        if c == 'f': return src if extra == '0' else 'FlexCat (Zune68 local stand-in)'
        name, num, s = msg
        padded = len(s) + 1 + ((len(s) + 1) & 1)
        if c == 'i': return name
        if c == 'd': return str(num)
        if c == 'e': return str(perline)
        if c == 's': return c_string(s)
        if c == 'a': return esc_bytes(num, width)
        if c == 't': return esc_bytes(padded, width)
        if c == 'z': return '\\x00' * (padded - len(s))
        return m.group(0)
    return CODE.sub(rep, line)


def per_string(line):
    return any(m.group(2) in 'idseatz' for m in CODE.finditer(line))


def generate(msgs, src, out, sd):
    base = Path(src).stem
    res = []
    for line in Path(sd).read_text('latin-1').split('\n'):
        if line.startswith('##'):
            continue
        line = line.replace('\\n', '\n') if '%' in line and per_string(line) else line
        if per_string(line):
            for k, msg in enumerate(msgs):
                res.append(subst(line, msg, base, src, k))
        else:
            res.append(subst(line, None, base, src, 0))
    Path(out).write_text('\n'.join(res), 'latin-1')


def main(argv):
    args = argv[1:]
    if 'CATALOG' in [a.upper() for a in args]:
        sys.stderr.write('flexcat stand-in: catalogs not supported\n')
        return 20
    src = args[0]
    msgs = parse_pot(src) if src.endswith(('.pot', '.po')) else parse_cd(src)
    for a in args[1:]:
        if '=' in a:
            out, sd = a.split('=', 1)
            generate(msgs, src, out, sd)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
