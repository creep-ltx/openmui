#!/usr/bin/env python3
"""Run the linked Area dispatcher's MUIA_Disabled on a set-up object, with
and without its live block (the event nodes only objects that react to
input get, classes/area.c).

Since the live block is made only for such objects, a label, image or group
has none while it is shown. Disabling or enabling it ran the simulated
button release, which wrote into the block through NULL (YAM 2.5's
configuration window reset the machine). Superclass and window methods are
modelled (check_notifications.NotifyHarness).
"""
import sys
from pathlib import Path
from check_class_lifetime import MASTER, STACK, low_memory, check_low_memory
from check_notifications import NotifyHarness, CLASS, OBJECT

SET, DISABLED = 0x103, 0x80423661
MADF_SETUP = 1 << 28
# struct MUI_AreaData behind Notify's 28 bytes (offsetof from the source):
# mad_RenderInfo 0, mad_Flags 36, mad_InputMode 89, mad_Live 98,
# mad_DisableCount 106
AREA = OBJECT + 28
FLAGS, INPUTMODE, LIVE, DISABLECOUNT = AREA + 36, AREA + 89, AREA + 98, AREA + 106
MRI, WINDOW = 0x74000, 0x75000


class AreaHarness(NotifyHarness):
    def __init__(self, path):
        super().__init__(path)
        self.mem.w16(CLASS + 32, 28)            # Area's data after Notify's
        self.mem.w32(AREA, MRI)                 # mad_RenderInfo
        self.mem.w32(MRI, WINDOW)               # mri_WindowObject
        self.mem.w32(FLAGS, MADF_SETUP)         # set up, cannot draw

    def super_method(self):
        self.cpu.w_reg(0, 0)

    def dispatch(self, method, *args):
        self.cpu.w_reg(8, CLASS)
        self.cpu.w_reg(10, OBJECT)
        self.cpu.w_reg(9, self.words(method, *args))
        self.cpu.w_reg(14, MASTER)
        self.machine.prepare(self.symbols['_Area_Dispatcher'], STACK)
        low = low_memory(self.mem)
        result = self.machine.execute(100000)
        check_low_memory(self.mem, low, 'Area OM_SET')
        assert self.machine.was_exit(result), 'area dispatcher'
        assert self.cpu.r_sp() == STACK

    def disable(self, on):
        self.events.clear()
        self.dispatch(SET, self.words(DISABLED, 1 if on else 0, 0, 0), 0)
        return [e[1] for e in self.events]


def check(path):
    h = AreaHarness(path)
    # a label: input mode None, no live block
    h.mem.w8(INPUTMODE, 0)
    h.mem.w32(LIVE, 0)
    assert h.disable(True) == [], 'no handler for an object without input'
    assert h.mem.r16(DISABLECOUNT) & 0x8000, 'disabled'
    assert h.disable(False) == []
    assert h.mem.r16(DISABLECOUNT) == 0
    # a button: its live block; the release puts its mouse handler back
    # (ehn.ehn_Events, offset 20 of the block, becomes IDCMP_MOUSEBUTTONS)
    live = h.blob(bytes(80))
    h.mem.w8(INPUTMODE, 1)
    h.mem.w32(LIVE, live)
    h.disable(True)
    assert h.mem.r32(live + 20) == 8, 'release ran on the live block'
    h.mem.w32(live + 20, 0)
    h.disable(False)
    assert h.mem.r32(live + 20) == 8, 'and on enabling'
    h.machine.cleanup()
    print(f'{path}: MUIA_Disabled on set-up objects with and without the live block')


if __name__ == '__main__':
    for filename in sys.argv[1:]:
        check(Path(filename))
