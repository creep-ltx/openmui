"""Offsets in Group's instance data (struct MUI_GroupData, group.c) that the
white-box Group checks use, from the start of the instance data (classic
m68k layout, checked with offsetof). One place to change when the layout
changes. Since the 68000 rewrite (round rw-group) a virtual group's state
is in the group's extra block (struct GroupExtra), at VIRT within it."""
CHILDLIST = 2           # childlist (struct MinList)
LAYOUT_HOOK = 14        # layout_hook
FLAGS = 18              # flags (ULONG)
EXTRA = 26              # x (struct GroupExtra *)
EXTRA_SIZE = 96         # a virtual group's extra block
VIRT = 28               # offsetof(struct GroupExtra, virt)
GROUP_CHANGING = 1 << 4
GROUP_VIRTUAL = 1 << 6
GROUP_UPDATE_SCROLL = 1 << 12   # MUI_Redraw(): the virtual offsets moved
GROUP_XFULL = 1 << 15
# struct GroupVirtual, from its start (VIRT within the extra block)
OFFX, OFFY, OLDX, OLDY, EXTW, EXTH = 24, 28, 32, 36, 40, 44
