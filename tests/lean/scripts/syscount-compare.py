#!/usr/bin/env python3
"""syscount-compare.py [-t] [-f FN,FN] [-o OP] LABEL=FILE ...: the OS calls per operation of
opbench "count"/"time" runs (tests/native/syscount.h) side by side.

Per operation: the time (ms) and, for every function any run called, the top-level calls per
operation (the calls the program and the library make themselves, not those an OS function makes
inside); -a adds all calls (also inside OS functions), -t the time inside the top-level calls (us).
-s prints only one summary line per operation: the top-level calls of all functions together.
"""
import re, sys

def load(path):
    ops, cur = {}, None
    times = {}
    for line in open(path, errors='replace'):
        m = re.match(r'(.{18}) us/op=\s*([\d.]+)', line)
        if m:
            times[m.group(1).strip()] = float(m.group(2)) / 1000
            continue
        if line.startswith('calls '):
            parts = line.rstrip('\n').split(' | ')
            name = parts[0][6:].strip()
            fns = {}
            for p in parts[1:]:
                f = p.split()
                fns[f[0]] = (float(f[1]), float(f[2]), int(f[3]))
            ops[name] = fns
    return times, ops

def main():
    args = sys.argv[1:]
    show_all = show_time = summary = False
    only_fn = only_op = None
    while args and args[0].startswith('-'):
        a = args.pop(0)
        if a == '-a': show_all = True
        elif a == '-t': show_time = True
        elif a == '-s': summary = True
        elif a == '-f': only_fn = set(args.pop(0).split(','))
        elif a == '-o': only_op = args.pop(0)
    runs = []
    for a in args:
        label, path = a.split('=', 1)
        runs.append((label,) + load(path))
    opnames = []
    for _, t, o in runs:
        for n in list(t) + list(o):
            if n not in opnames:
                opnames.append(n)
    labels = [r[0] for r in runs]
    if summary:
        print('| operation | ' + ' | '.join('%s ms / calls' % l for l in labels) + ' |')
        print('|---' * (len(labels) + 1) + '|')
    for op in opnames:
        if only_op and only_op != op:
            continue
        if summary:
            cells = []
            for _, t, o in runs:
                fns = o.get(op, {})
                tot = sum(v[0] for k, v in fns.items()
                          if not only_fn or k in only_fn)
                cells.append('%s / %.1f' % ('%.2f' % t[op] if op in t else '-', tot))
            print('| %s | %s |' % (op, ' | '.join(cells)))
            continue
        print('## %s   ms: %s' % (op, '  '.join('%s %s' % (l, '%.2f' % t[op] if op in t else '-')
                                              for l, t, _ in runs)))
        fnames = []
        for _, _, o in runs:
            for f in o.get(op, {}):
                if f not in fnames and (not only_fn or f in only_fn):
                    fnames.append(f)
        print('  %-22s' % 'function' + ''.join('%14s' % l for l in labels))
        for f in fnames:
            row = []
            for _, _, o in runs:
                v = o.get(op, {}).get(f)
                if not v:
                    row.append('%14s' % '.')
                    continue
                s = '%.2f' % v[0]
                if show_all: s += '/%.0f' % v[1]
                if show_time: s += ' %dus' % v[2]
                row.append('%14s' % s)
            print('  %-22s' % f + ''.join(row))
        print()

main()
