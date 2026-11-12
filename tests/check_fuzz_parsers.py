#!/usr/bin/env python3
"""Fuzz the parsers that read outside input, on a 68000 in the harness (gate 2, robust input).

Random strings at the very end of emulated RAM (a read past the terminator leaves RAM and
stops the machine: the harness reports the call as not returned) are given to
  - zune_pen_spec_to_intern      (pen specs: "r...", "m...", "s...", "p...")
  - zune_frame_spec_to_intern    (frame specs: six to eight hex digits)
  - zune_gradient_string_to_intern (gradient specs: "H,rrr,ggg,bbb-rrr,ggg,bbb")
  - zune_text_new / zune_text_destroy (the text engine's escape sequences, except \\33O[...],
    which carries a pointer by MUI's definition and is never outside input)
Each parser must return (accept or reject), write only its output, and leave no allocation
behind. FUZZ_CASES (default 400 per parser) and FUZZ_SEED (default 1) in the environment;
a failing input is printed with the seed so that it can be run again.
"""
import os
import random
import sys
from pathlib import Path
from check_class_lifetime import Harness, STACK, MASTER
from check_textengine import TextHarness, PREPARSE

CASES = int(os.environ.get('FUZZ_CASES', '400'))
SEED = int(os.environ.get('FUZZ_SEED', '1'))
OUTPUT = 0x70000
DOS = 0x76000


class FuzzHarness(Harness):
    """A harness whose call() reports instead of asserting, with DOS StrToLong and the
    Utility arithmetic the parsers may use."""

    def __init__(self, path):
        super().__init__(path)
        for name in ('_UtilityBase', '___UtilityBase'):
            if name in self.symbols:
                self.mem.w32(self.symbols[name], 0x78000)
        self.trap(0x78000 - 138, lambda: self.cpu.w_reg(0,
            (self.cpu.r_reg(0) * self.cpu.r_reg(1)) & 0xffffffff))
        self.trap(0x78000 - 156, self.divide)
        for name in ('_DOSBase', '___DOSBase'):
            if name in self.symbols:
                self.mem.w32(self.symbols[name], DOS)
        self.trap(DOS - 816, self.str_to_long)
        self.ram_end = self.machine.ram_total

    def divide(self):
        d = self.cpu.r_reg(1) or 1
        q, r = divmod(self.cpu.r_reg(0), d)
        self.cpu.w_reg(0, q & 0xffffffff)
        self.cpu.w_reg(1, r & 0xffffffff)

    def str_to_long(self):
        """dos StrToLong(string a0, value a1): characters read, -1 for none."""
        s, out = self.cpu.r_reg(8), self.cpu.r_reg(9)
        i = 0
        while s + i < self.ram_end and self.mem.r8(s + i) in b' \t':
            i += 1
        neg = False
        if s + i < self.ram_end and self.mem.r8(s + i) in b'+-':
            neg = self.mem.r8(s + i) == ord('-'); i += 1
        start, v = i, 0
        while s + i < self.ram_end and 48 <= self.mem.r8(s + i) <= 57:
            v = v * 10 + self.mem.r8(s + i) - 48; i += 1
        if i == start:
            self.cpu.w_reg(0, 0xffffffff)
            return
        self.mem.w32(out, (-v if neg else v) & 0xffffffff)
        self.cpu.w_reg(0, i)

    def place(self, data):
        """data (bytes, terminated here) at the end of RAM; returns its address."""
        data = data + b'\0'
        address = self.ram_end - len(data)
        self.mem.w_block(address, data)
        return address

    def run(self, name, *args):
        """call name; (returned, d0): returned False when the machine stopped elsewhere
        (a read past the end of RAM, an exception) or the stack came back wrong."""
        for i, value in enumerate(args):
            self.mem.w32(STACK + 4 * i, value)
        self.cpu.w_reg(14, MASTER)
        self.cpu.w_reg(0, 21)
        self.machine.prepare(self.symbols['_' + name], STACK)
        try:
            result = self.machine.execute(getattr(self, 'cycle_budget', 200000))
        except Exception as e:   # a read outside RAM stops the machine with an error
            print(f'  {name}: {e}')
            self.lock_depth = 0
            return False, 0
        ok = self.machine.was_exit(result) and self.cpu.r_sp() == STACK and self.lock_depth == 0
        self.lock_depth = 0
        return ok, self.cpu.r_reg(0)


def rand_string(rnd, alphabet, maxlen):
    n = rnd.randint(0, maxlen)
    return bytes(rnd.choice(alphabet) for _ in range(n))


def fuzz_simple(h, rnd, name, alphabet, maxlen, out_size, label):
    fails = 0
    for i in range(CASES):
        s = rand_string(rnd, alphabet, maxlen)
        if rnd.random() < 0.1:
            s = bytes(rnd.randrange(1, 256) for _ in range(rnd.randint(1, maxlen)))
        address = h.place(s)
        h.mem.w_block(OUTPUT - 16, b'!' * 16)
        h.mem.w_block(OUTPUT, bytes(out_size))
        h.mem.w_block(OUTPUT + out_size, b'!' * 16)
        ok, d0 = h.run(name, address, OUTPUT)
        guard = (bytes(h.mem.r_block(OUTPUT - 16, 16)) == b'!' * 16
                 and bytes(h.mem.r_block(OUTPUT + out_size, 16)) == b'!' * 16)
        same = bytes(h.mem.r_block(address, len(s))) == s
        if not ok or not guard or not same or h.allocations:
            fails += 1
            print(f'{label}: case {i} seed {SEED} input {s!r}: '
                  f'{"did not return" if not ok else ""} {"wrote outside its output" if not guard else ""} '
                  f'{"changed its input" if not same else ""} {"left allocations" if h.allocations else ""}')
            h.allocations.clear()
            if fails > 5:
                break
    return fails


def fuzz_text(h, rnd):
    """the text engine: escapes, image specs, STATE=, tabs, newlines, random bytes"""
    esc = b'crlnUubitTsSgGBFVvhHI'
    fails = 0
    for i in range(CASES):
        parts = []
        for _ in range(rnd.randint(0, 12)):
            r = rnd.random()
            if r < 0.4:
                parts.append(b'\33' + bytes([rnd.choice(esc)]))
            elif r < 0.55:
                parts.append(b'\33I[' + rand_string(rnd, b'0123456789:,=STATE/abc', 12) + (b']' if rnd.random() < 0.8 else b''))
            elif r < 0.6:
                parts.append(b'\33' + bytes([rnd.randrange(1, 256)]))
            elif r < 0.7:
                parts.append(rnd.choice([b'\n', b'\t', b'\33', b'[', b']', b',']))
            else:
                parts.append(rand_string(rnd, b'abc xyz019', 6))
        content = b''.join(parts)
        if b'\33O' in content:
            content = content.replace(b'\33O', b'\33o')
        preparse = b''.join(b'\33' + bytes([rnd.choice(esc)]) for _ in range(rnd.randint(0, 3)))
        h.mem.w_block(PREPARSE, preparse + b'\0')
        address = h.place(content)
        h.allocs = 0
        if not h.allocations:      # the harness's heap only grows: start over when it is empty
            h.next_address = 0x80000
        ok, text = h.run('zune_text_new', PREPARSE, address, 0, 0)
        if ok and text:
            ok2, _ = h.run('zune_text_destroy', text)
            ok = ok and ok2
        if not ok or h.allocations:
            fails += 1
            print(f'text engine: case {i} seed {SEED} preparse {preparse!r} content {content!r}: '
                  f'{"did not return" if not ok else "left allocations"}')
            h.allocations.clear()
            if fails > 5:
                break
    return fails


class FuzzTextHarness(TextHarness, FuzzHarness):
    pass


def check(path):
    rnd = random.Random(SEED)
    h = FuzzHarness(path)
    fails = 0
    try:
        fails += fuzz_simple(h, rnd, 'zune_pen_spec_to_intern', b'rmsp0123456789abcdefABCDEF,;+- \t', 40, 48, 'pen spec')
        fails += fuzz_simple(h, rnd, 'zune_frame_spec_to_intern', b'0123456789abcdefABCDEFgz ', 12, 16, 'frame spec')
        fails += fuzz_simple(h, rnd, 'zune_gradient_string_to_intern', b'HhVv0123456789abcdefABCDEF,-+ xyz', 48, 160, 'gradient spec')
    finally:
        h.machine.cleanup()
    t = FuzzTextHarness(path)
    try:
        fails += fuzz_text(t, rnd)
    finally:
        t.machine.cleanup()
    assert not fails, f'{fails} parser failures'
    print(f'{path.name}: {4 * CASES} fuzzed parser inputs passed (seed {SEED})')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))
