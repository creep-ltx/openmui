#!/usr/bin/env python3
# failsym.py LIBRARY [TSV...]: name the library functions in failsweep's reports. LIBRARY is the
# build with symbols (not linked with -s); each report line carries the addresses of the failed
# call's callers and "muimaster LO-HI LO-HI" (the library's first two code hunks as loaded);
# an address inside the first hunk becomes "function+offset" through m68k-amigaos-nm. Lines
# whose status is not ok/nowindow, or whose "lost" is below -1000 bytes, are printed with the
# names; with --all every line. Reads the merged TSV (N, status, windows, lost, report).
import bisect
import re
import subprocess
import sys


def load_symbols(lib):
    out = subprocess.run(['m68k-amigaos-nm', '-n', lib], capture_output=True, text=True, check=True).stdout
    syms = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in 'tT':
            syms.append((int(parts[0], 16), parts[2]))
    syms.sort()
    return [a for a, _ in syms], [n for _, n in syms]


def name(addrs, names, lo, hi, a):
    if not (lo <= a < hi):
        return '%08x' % a
    off = a - lo
    i = bisect.bisect_right(addrs, off) - 1
    if i < 0:
        return '%08x' % a
    return '%s+%x' % (names[i].lstrip('_'), off - addrs[i])


def main():
    args = sys.argv[1:]
    show_all = '--all' in args
    args = [a for a in args if a != '--all']
    addrs, names = load_symbols(args[0])
    for tsv in args[1:]:
        for line in open(tsv):
            f = line.rstrip('\n').split('\t')
            if len(f) < 5:
                continue
            n, status, wl, lost, rep = f[:5]
            try:
                lost = int(lost)
            except ValueError:
                lost = 0
            if not show_all and status in ('ok', 'nowindow') and lost > -1000:
                continue
            m = re.search(r'failed N=(\d+) (\S+) ret=([0-9A-Fa-f]+) callers ([0-9A-Fa-f]+) ([0-9A-Fa-f]+) ([0-9A-Fa-f]+) muimaster ([0-9A-Fa-f]+)-([0-9A-Fa-f]+)', rep)
            if not m:
                print(f'{n}\t{status}\t{lost}\t(no report){" " + rep[:100] if rep else ""}')
                continue
            lo, hi = int(m.group(7), 16), int(m.group(8), 16)
            calls = [name(addrs, names, lo, hi, int(m.group(k), 16)) for k in (3, 4, 5, 6)]
            print(f'{n}\t{status}\t{lost}\t{m.group(2)}\tret {calls[0]}\tcallers {calls[1]} {calls[2]} {calls[3]}')


if __name__ == '__main__':
    main()
