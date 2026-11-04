# Zune68, lean edition

Branch `lean`: Zune68 with the memory diet of Zune68k ported, so that it
reports MUI 5 (21.227) and stays close to MUI 3.8 in memory. The goal:
stock OS 3.1 works (68000 build the default, 68020 too), every feature
programs use is kept, Zune68's behaviour is kept (smart refresh, as MUI
3.8), and only MUI 5-only parts and rarely used classes become on-demand.

MUI-Demo, cold, OS 3.1 + 4 MB fast (FS-UAE, 68000 build):

| | library alone | first program | each further |
|---|---|---|---|
| MUI 3.8 | 238 KB | 460 KB | 211-219 KB |
| Zune68 main | 535 KB | 832 KB | 334 KB |
| lean | 331 KB | 601 KB | 266 KB |

Details: [measurements.md](measurements.md), the class split:
[class-split.md](class-split.md).

## What differs from upstream

One commit per change, each meant to be able to stand as its own pull
request (later ones are written on top of the earlier ones, see "Depends"):

| commit | change | saving (MUI-Demo, OS 3.1) | depends |
|---|---|---|---|
| be74ee7d `muimaster: open rexxsyslib.library v36 so it starts on stock OS 3.1` | 3.1 has rexxsyslib 36; asking for 37 made the library fail to open | makes it open at all | - |
| 294fa0af `muimaster: open optional libraries on first use` | asl, commodities, rexxsyslib, datatypes, iffparse, diskfont, icon, workbench and the IEEE double libraries opened when a feature needs them (`lazylib.h`); a missing one fails only its feature; fonts in memory come from OpenFont | library alone -104 KB, first program -79 KB | - |
| 7f31a15e `muimaster: smaller per-application and per-list memory` | window positions on demand (was 6 KB per application), one block per notification, List title entry outside the pool, 1 KB list and window pools, list columns reserved as needed (at least 16) instead of 64 | each further -33 KB | 294fa0af (one line in notify.c) |
| 2475c0fd `Area: make the event and timer nodes at the first setup` | event/timer nodes, click position and context menu state in a block made at first setup, kept until dispose; Area 238 -> 136 bytes | each further -35 KB | - |
| f949dc3d `muimaster: MUI 3.8 behaviour that TheBar and YAM rely on` | MUIA_Group_Horiz settable after OM_NEW, MUIA_KnowsDisabled, draw mode restored after JAM2 drawing, Numericbutton formatting codes not drawn | none (fixes TheBar demo2/demo3, YAM labels) | - |
| cdecd1a7 `muimaster: rarely used and MUI 5-only classes as files loaded on demand` | 35 classes as `LIBS:Zune/<Name>.mui`, made through the built-in path; release and installer carry them | library file -109 KB, library alone -102 KB, first program -85 KB | 294fa0af |

The tools and reports (`tests/lean/`, `tools/lean/`, `docs/lean/`) are
for measuring only; they are not part of any of these changes.

## Left open

- Per-object memory is still ~50 KB per program above MUI 3.8: one Family
  object per Group (~7.5 KB in MUI-Demo), and Zune68's larger Text, String
  and List instance data (measurements.md, step 7).
- NListtree-Demo opens no window (Zune68 main too).
- `make check` (the amitools-based harness) was not run: it needs the
  Python bundled with Stefan's toolchain.
- Tested in FS-UAE only, not on real hardware.
