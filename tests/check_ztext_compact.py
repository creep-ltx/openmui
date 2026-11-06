#!/usr/bin/env python3
"""Check zune_text_compact() on linked 68000 code: a kept parse moves into a
block of the size it used, every list link and inline string with it, lines
and chunks outside the arena stay and are relinked, and destroy frees all."""
import sys
from pathlib import Path
from check_class_lifetime import Harness, EXEC

TEXTS = 0x74000


class TextHarness(Harness):
    def __init__(self, path):
        super().__init__(path)
        self.trap(EXEC - 624, lambda: self.mem.w_block(self.cpu.r_reg(9),
            self.mem.r_block(self.cpu.r_reg(8), self.cpu.r_reg(0))))  # CopyMem
        self.trap(EXEC - 264, self.rem_tail)  # RemTail
        self.next_text = TEXTS

    def rem_tail(self):
        head = self.cpu.r_reg(8)
        node = self.mem.r32(head + 8)
        if self.mem.r32(node + 4) == 0:  # empty: TailPred is the head
            self.cpu.w_reg(0, 0)
            return
        pred = self.mem.r32(node + 4)
        self.mem.w32(pred, head + 4)
        self.mem.w32(head + 8, pred)
        self.cpu.w_reg(0, node)

    def cstring(self, s):
        address = self.next_text
        self.next_text += (len(s) + 4) & ~3
        self.mem.w_block(address, s + b'\0')
        return address

    def read_string(self, address, length):
        return bytes(self.mem.r_block(address, length))

    def walk(self, text):
        """[(line, [(chunk, str, len)])] following the links forward, and
        check every backward link on the way"""
        lines = []
        head = text + 4  # ZText: width, height, the line list, arena_next (16), arena_end (20)
        prev, node = head, self.mem.r32(head)
        while self.mem.r32(node):
            assert self.mem.r32(node + 4) == prev, ('line pred', hex(node))
            chunks = []
            chead = node + 10  # ZTextLine: node, align, lshift, chunklist
            cprev, chunk = chead, self.mem.r32(chead)
            while self.mem.r32(chunk):
                assert self.mem.r32(chunk + 4) == cprev, ('chunk pred', hex(chunk))
                s, n = self.mem.r32(chunk + 8), self.mem.r16(chunk + 42)
                chunks.append((chunk, s, self.read_string(s, n) if s else None))
                cprev, chunk = chunk, self.mem.r32(chunk)
            assert chunk == chead + 4 and self.mem.r32(chead + 8) == cprev
            lines.append((node, chunks))
            prev, node = node, self.mem.r32(node)
        assert node == head + 4 and self.mem.r32(head + 8) == prev
        return lines


def check(path):
    for preparse, content in [(b'\x1bc', b'_Ok'), (b'\x1bc', b'Button'),
                              (b'', b'\x1bbBold\x1bn and plain\nsecond line'),
                              (b'\x1bc', b'X' * 150 + b'\x1bbY')]:
        h = TextHarness(path)
        text = h.call('zune_text_new', h.cstring(preparse) if preparse else 0,
                      h.cstring(content), 0, 0)
        assert text and text in h.allocations
        before = [[s for _, _, s in chunks] for _, chunks in h.walk(text)]
        outside = {c for _, chunks in h.walk(text) for c, _, _ in chunks
                   if c not in range(text, h.mem.r32(text + 20))}
        size = h.allocations[text]
        new = h.call('zune_text_compact', text)
        assert new and new in h.allocations
        arena_next, arena_end = h.mem.r32(new + 16), h.mem.r32(new + 20)
        assert arena_next == arena_end == new + h.allocations[new]
        # each of these leaves at least 16 bytes of its arena unused
        assert new != text, ('not compacted', content)
        assert text not in h.allocations, 'old block not freed'
        assert h.allocations[new] < size
        if len(content) > 100:
            assert outside, 'the long chunk should have come from outside the arena'
        after = h.walk(new)
        assert [[s for _, _, s in chunks] for _, chunks in after] == before, (content, before)
        for line, chunks in after:
            assert not (text <= line < text + size) or new == text, 'line left behind'
            for chunk, s, _ in chunks:
                if chunk in outside:
                    continue
                assert new <= chunk < arena_end or new == text, 'chunk left behind'
                assert s == 0 or new <= s < arena_end or new == text, 'inline string left behind'
        h.call('zune_text_destroy', new)
        assert not h.allocations, ('left allocated', h.allocations)
        h.machine.cleanup()


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))
    print('zune_text_compact: kept parses moved into their size, links and strings moved, all freed')
