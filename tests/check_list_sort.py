#!/usr/bin/env python3
"""Exercise linked sorting and count comparison work; no OS timing claims."""
import random
import struct
import sys
from pathlib import Path
from check_notifications import NotifyHarness
from check_class_lifetime import STACK, MASTER, EXEC, INTUITION

CLASS, OBJECT, DATA = 0x70000, 0x71000, 0x71100
# struct MUI_ListData (classes/list_private.h): the offsets the test uses
POOL, ENTRIES_NUM, ENTRIES_ALLOCATED, ENTRIES = 16, 40, 44, 48
ENTRIES_ACTIVE, INSERT_POSITION, CONFIRM_ENTRIES_NUM = 60, 64, 96
# struct ListEntry: data, height (WORD), flags (UWORD); the rows are an array
ENTRY_SIZE = 8


class SortHarness(NotifyHarness):
    def __init__(self, path, values):
        super().__init__(path)
        self.mem.w16(CLASS + 32, 256)
        self.mem.w32(DATA, 32)  # LIST_QUIET
        self.comparisons = 0
        # an entry is its data, a key number: the rows hold it in place
        self.entries = list(range(1, len(values) + 1))
        self.keys = {n + 1: value for n, value in enumerate(values)}
        self.array = self.rows(self.entries)
        self.mem.w32(DATA + ENTRIES_NUM, len(values))
        self.mem.w32(DATA + ENTRIES, self.array)
        self.mem.w32(DATA + ENTRIES_ACTIVE, 0 if values else 0xffffffff)
        self.trap(self.symbols['_DoMethodA'], self.compare)
        self.trap(self.symbols['_MUI_Redraw'], lambda: None)

    def rows(self, entries, spare=1):
        return self.blob(b''.join(struct.pack('>IHH', e, 0, 0)
                                  for e in entries) + bytes(ENTRY_SIZE * spare))

    def row(self, n):
        return self.mem.r32(self.array + n * ENTRY_SIZE)

    def compare(self):
        msg = self.argument(1)
        assert self.mem.r32(msg) == 0x80421b68  # MUIM_List_Compare
        a, b = self.keys[self.mem.r32(msg + 4)], self.keys[self.mem.r32(msg + 8)]
        self.cpu.w_reg(0, ((a > b) - (a < b)) & 0xffffffff)
        self.comparisons += 1

    def sort(self):
        for n, arg in enumerate([CLASS, OBJECT, self.words(0x80422275)]):
            self.mem.w32(STACK + 4 * n, arg)
        self.cpu.w_reg(14, MASTER)
        self.machine.prepare(self.symbols['_List__MUIM_Sort'], STACK)
        assert self.machine.was_exit(self.machine.execute(100000000))
        assert not self.allocations, 'sort must not allocate'
        return [self.row(n) for n in range(len(self.entries))]


class InsertHarness(SortHarness):
    def __init__(self, path, values):
        super().__init__(path, values)
        # the title row first, room for 32 rows
        self.array = self.rows([0] + self.entries, 32 - len(values)) + ENTRY_SIZE
        self.mem.w32(DATA + ENTRIES, self.array)
        self.mem.w32(DATA + ENTRIES_ALLOCATED, 33)
        self.mem.w32(DATA + CONFIRM_ENTRIES_NUM, len(values))
        self.mem.w32(DATA + POOL, 123)  # caller-owned entry pool
        self.fail_construct = 0
        self.constructs = 0
        self.trap(EXEC - 708, self.alloc_vec)
        self.trap(EXEC - 714, self.free_vec)
        self.trap(INTUITION - 648, self.set_attrs)
        self.trap(self.symbols['_DoSuperMethodA'], lambda: self.cpu.w_reg(0, 0))
        self.dispatch_stub = self.blob(bytes(8))
        self.trap(self.dispatch_stub, self.method)
        self.mem.w16(self.dispatch_stub + 2, 0x4ef9)
        self.mem.w16(self.symbols['_DoMethod'], 0x4ef9)
        self.mem.w32(self.symbols['_DoMethod'] + 2, self.dispatch_stub)
        self.sort_script = self.script(('List__MUIM_Sort', self.words(0x80422275)))

    def method(self):
        method = self.argument(1)
        target = self.ret
        if method == 0x8042d662:
            self.constructs += 1
            value = self.argument(2)
            self.cpu.w_reg(0, 0 if self.constructs == self.fail_construct else value)
        elif method == 0x80422275:
            target = self.sort_script
        else:
            raise AssertionError(hex(method))
        self.mem.w32(self.dispatch_stub + 4, target)

    def set_attrs(self):
        tags = self.cpu.r_reg(9)
        while self.mem.r32(tags):
            attr, value = self.mem.r32(tags), self.mem.r32(tags + 4)
            if attr == 0x80421654:
                self.mem.w32(DATA + ENTRIES_NUM, value)
            elif attr == 0x8042391c:  # List_Active
                self.mem.w32(DATA + ENTRIES_ACTIVE, value)
            tags += 8
        self.cpu.w_reg(0, 0)

    def insert(self, values, position=-2):
        incoming = []
        for value in values:
            key = max(self.keys, default=0) + 1
            self.keys[key] = value
            incoming.append(key)
        message = self.words(0x80426c87, self.words(*incoming), len(values),
                             position & 0xffffffff)
        result = self.call('List__MUIM_Insert', CLASS, OBJECT, message)
        count = self.mem.r32(DATA + ENTRIES_NUM)
        entries = [self.row(n) for n in range(count)]
        return result, entries

    def finish(self):
        for address in list(self.allocations):
            self.call('FreeVecPooled', 123, address + 4)
        self.check_freed()
        self.machine.cleanup()


def check(path, baseline=False):
    if not baseline:
        for values, incoming in [([2, 4], 1), ([2, 4], 3), ([2, 4], 5),
                                 ([2, 2, 4], 2), ([], 1)]:
            h = InsertHarness(path, values)
            result, entries = h.insert([incoming])
            assert [h.keys[e] for e in entries] == sorted(values + [incoming])
            assert h.mem.r32(DATA + INSERT_POSITION) == sum(v <= incoming for v in values)
            if values:
                assert entries[h.mem.r32(DATA + ENTRIES_ACTIVE)] == h.entries[0]
            assert h.comparisons <= 3
            h.finish()
        h = InsertHarness(path, list(range(28)))
        result, entries = h.insert([-1])
        assert h.comparisons <= 5, 'sorted insertion must not recheck the prefix'
        h.finish()
        h = InsertHarness(path, [2, 4])
        result, entries = h.insert([1, 3])
        assert [h.keys[e] for e in entries] == [1, 2, 3, 4]
        assert h.mem.r32(DATA + INSERT_POSITION) == 0, 'first inserted entry position'
        assert entries[h.mem.r32(DATA + ENTRIES_ACTIVE)] == h.entries[0]
        h.finish()
        h = InsertHarness(path, [2, 4])
        h.mem.w32(DATA + ENTRIES_ALLOCATED, 3)  # full: the rows must grow
        h.fail_alloc = True
        result, entries = h.insert([1], 0)
        assert result == 0xffffffff and entries == h.entries
        h.finish()
        for failure in [1, 2, 3]:
            h = InsertHarness(path, [2, 4])
            h.fail_construct = failure
            result, entries = h.insert([1, 3, 5], 0)
            expected = [1, 3, 5][:failure - 1] + [2, 4]
            assert result == 0xffffffff
            assert [h.keys[e] for e in entries] == expected
            assert entries[h.mem.r32(DATA + ENTRIES_ACTIVE)] == h.entries[0]
            h.finish()

    rng = random.Random(68)
    cases = [[], [1], [2, 1], [3, 1, 2], [1, 2, 2, 3, 2],
             list(range(128)), list(reversed(range(128))),
             [rng.randrange(16) for _ in range(128)]]
    for values in cases:
        h = SortHarness(path, values)
        active = h.entries[0] if values else None
        result = h.sort()
        assert sorted(result) == sorted(h.entries)
        assert [h.keys[e] for e in result] == sorted(values)
        if active:
            assert result[h.mem.r32(DATA + ENTRIES_ACTIVE)] == active
        if not baseline:
            assert h.comparisons <= max(1, len(values) * 20)
            if values == sorted(values):
                assert h.comparisons == max(0, len(values) - 1)
        h.machine.cleanup()

    h = SortHarness(path, list(range(128)))
    total = 0
    for count in range(1, 129):
        h.mem.w32(DATA + ENTRIES_NUM, count)
        h.sort()
    total = h.comparisons
    h.machine.cleanup()
    print(f'{path.name}: sorting, duplicate keys and active identity pass; '
          f'128 growing sorted prefixes: {total} comparisons')


if __name__ == '__main__':
    baseline = '--baseline' in sys.argv
    for name in sys.argv[1:]:
        if name != '--baseline':
            check(Path(name), baseline)
