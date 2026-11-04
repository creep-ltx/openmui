# Lean edition: where Zune68's memory goes (2026-11-04)

Step 1 of the lean edition: build Zune68, measure it the same way as MUI 3.8 and
Zune68k (an earlier lean Zune port for OS 3.1), and find where its memory goes.
Nothing in the library is changed by this step; the tools are in `tools/lean/` and
`tests/lean/`. All figures are from FS-UAE (emulated A1200 68EC020 / A600 68000),
not real hardware.

## Building

- `git submodule update --init --recursive`, then `tests/lean/scripts/build.sh`
  (`make core` twice: with symbols and a link map for the memory reports, and
  linked with `-s`). Released builds use GCC 16.2 (CI container); bebbo's
  GCC 6.5 also builds the library unchanged with `CFLAGS=-std=gnu11 ...` (it
  has no `-std=gnu17`). Code from 6.5 is ~1.5% larger; memory figures match
  the Beta 2 release within 1%.
- **bebbo's 6.5 `m68k-amigaos-strip --strip-all` drops the relocations** of a
  hunk file (0 relocs left, the library would crash): link with `-s` instead.
- No FlexCat? `tools/lean/flexcat-sd.py` (named `flexcat` on `PATH`) writes the
  C headers from `.cd`/`.pot` + `.sd` files; it does not make catalogs, so
  build `core` (and plugins), not `all`/`release`.
- Build options: `CPUFLAGS` (default `-m68000 -msoft-float`), `MUI5_TEST=1`
  (reports 21.227, default) or `0` (19.81, same code), `LTO=1` (~20 KB smaller,
  breaks hunk symbols), `TRACE=1` (diagnostics).
- **Stock OS 3.1:** `muimaster_init.c` asks for `rexxsyslib.library` 37; 3.1 has
  36.23, so Beta 2 does not open at all on 3.1. For these measurements that one
  number was changed to 36 in the measured builds (not committed).

## The numbers (MUI-Demo, cold boot, `tests/lean/scripts/measure.sh`)

OS 3.1, A1200 68EC020, 2 MB chip + 4 MB fast:

| | library file | library only | first MUI-Demo | each further | first window |
|---|---|---|---|---|---|
| MUI 3.8 | 166 KB (+271 KB classes) | 238 KB (\*) | 460 KB | 211-219 KB | 1.7 s |
| Zune68k | 237 KB (+118 KB .mui) | 257 KB (\*) | 460-471 KB | 210-214 KB | 1.9-2.1 s |
| Zune68 main, 68000 | 429,472 | 535 KB | 832 KB | 334 KB | 2.0-2.1 s |
| Zune68 main, 68020 | 419,812 | 527 KB | 825 KB | 334 KB | 1.9 s |
| Zune68 main, 68000, `MUI5_TEST=0` | 429,412 | 535 KB | 832 KB | 334 KB | 2.0 s |
| Zune68 mui38-dev, 68000 | 343,748 | 452 KB | 746 KB | 314 KB | 2.4 s |
| Zune68 mui38-dev, 68020 | 336,652 | 447 KB | 740 KB | 314 KB | 2.1-2.3 s |
| Zune68 Beta 2 (Aminet) | 420,648 | does not open (rexxsyslib 37) | - | - | - |

(\*) measured earlier on the 2 MB chip-only machine.

OS 3.2.1, A1200 68EC020, 2 MB chip + 8 MB fast:

| | library only | first MUI-Demo | each further | first window |
|---|---|---|---|---|
| Zune68k | - | 460 KB | 214 KB | 1.9 s |
| Zune68 Beta 2 (Aminet, GCC 16.2, 68000) | 437 KB | 732 KB | 337 KB | 1.9 s |
| Zune68 main, 68000 (GCC 6.5) | 444 KB | 739 KB | 337 KB | 2.0 s |
| Zune68 main, 68020 (GCC 6.5) | 436 KB | 731 KB | 337 KB | 1.9-2.0 s |
| Zune68 mui38-dev, 68020 | 356 KB | 647 KB | 316 KB | 2.2-2.4 s |

A600 68000 7 MHz, Kickstart/Workbench 3.1, 1 MB chip + 4 MB fast, main 68000
build: works; first MUI-Demo 832 KB, each further 334 KB, first window 6.5-7.8 s.

On 3.1 the library alone takes ~90 KB more than on 3.2.1: the libraries it opens
at start are disk-based there.

## What opens at start (`wasabi debug --with-snoop`, `libsopen.sh`)

`L_InitLib` opens, and fails without: dos, utility, graphics, intuition,
**asl**, layers, **commodities**, **rexxsyslib (37)**, gadtools, keymap, locale,
**datatypes** (which pulls iffparse), **iffparse**, **diskfont**, icon, workbench,
then **mathieeedoubbas + mathieeedoubtrans** (`ZuneGccInit`). It installs an
in-memory `muigfx.library` 20.1. On 3.1 asl (41 KB), datatypes (16 KB),
diskfont (15 KB), iffparse (7 KB), rexxsyslib (33 KB), commodities (9 KB) and
mathieeedoubtrans (17 KB) are loaded from disk. MUI 3.8 with MUI-Demo running has
no asl, datatypes or diskfont in memory. The first program also loads
`FONTS:helvetica/9` (Zune's default title/tiny font).

## Where a further window's memory goes (`tally.sh` + `memtally`)

The second MUI-Demo runs under `tests/lean/memtally` (AllocMem/FreeMem patched,
live blocks of its task kept with return addresses), grouped with
`tools/lean/mtreport2.py DUMP LIB cats` against the symbol build. OS 3.1:

| | Zune68 main | mui38-dev | Zune68k |
|---|---|---|---|
| **total held by the task** | **258.8 KB / 1,639 blocks** | 238.6 KB / 2,159 | 137.3 KB / 1,318 |
| objects (instance data) | 161.4 KB / 504 | 156.5 KB / 504 | 89.5 KB / 437 |
| lists (pools, entries, columns) | 42.9 KB / 86 | 36.3 KB / 88 | 19.3 KB / 96 |
| notifications | 16.7 KB / 529 | 16.7 KB / 529 | 12.1 KB / 262 |
| Notify help blocks (one per object) | - | 8.0 KB / 501 | - |
| smart refresh backing store (intuition) | 17.1 KB | 0.3 KB | 0.3 KB |
| windows | 6.3 KB | 6.5 KB | 3.4 KB |
| text engine, strings, Text contents | 11.2 KB | 11.3 KB | 10.0 KB |
| image specs | 1.9 KB | 2.0 KB | 1.6 KB |

- **Objects**: Area's instance data is 238 bytes (Zune68k: 112) because every
  object carries three `MUI_EventHandlerNode`s, an `MUI_InputHandlerNode`, the
  context-menu fields and click position. ~126 bytes x 504 objects = ~63 KB.
  Instance sizes (bytes, `tools/lean/classsizes.py`), main / mui38-dev /
  Zune68k: Area 238/234/112, Application 6,348/6,334/330 (`winpos[]`), Window
  726/698/656, List 412/354/324, String 304/256/248, Group 152/148/136, Text
  50/30/22, Menu(item) 40/24/24. Zune68 also makes ~67 more objects for
  MUI-Demo (13 more Family objects; the rest not yet identified).
- **Lists**: every List's title entry opens a 2 KB pool puddle (13 x 2 KB), two
  Volumelist 2 KB blocks, and 64 display columns per list (`ParseListFormat`,
  ~8 KB in MUI-Demo).
- **Notifications**: two blocks per notification (node + parameters).
- **Smart refresh**: main defaults to smart refresh as MUI 3.8 does (commit
  a4ad85ae); the measured second window covers the first, so intuition keeps
  ~17 KB of backing store. MUI 3.8 pays this too; Zune68k and mui38-dev use
  simple refresh, so their per-window figures are ~17 KB flattered.
- **Windows**: `Window__MUIM_AllocGadgetID` holds a 4,136-byte block per window.

## Where the library size goes

Code + data per source, main against mui38-dev (68000, GCC 6.5): 395 KB against
316 KB. The ~80 KB added since mui38-dev split: String +11.3 KB (Textinput API,
multi-line), Fontpanel 9.4, List +7.5, textengine +5.4, Title 4.9, Filepanel
4.7, Window +3.9, Keyadjust 3.8, requesters 3.4, Menuitem +3.0, pointer 2.6,
Datamap 2.0, menu +1.8, Text +1.7, rtg 1.7, Textdata 1.6, imspec_vector +1.5,
Selectgroup 1.3, Process 1.3, SelectorPanel 0.9.

The 25 classes Zune68k loads as `.mui` files on demand are 72 KB of code in
Zune68 main; the new MUI 4/5 classes (Fontpanel, Filepanel, SelectorPanel,
Title, Keyadjust, Datamap/Objectmap, Textdata, Selectgroup, Process) another
30 KB.

## No memory lost (`fullcheck.sh`)

All 19 MUI 3.8 demos open and close on main (68000 build, OS 3.1), and
MUI-Demo's eight sub-windows open (on Zune68 the first button row is 126 pixels
above the window's bottom, not 111: `ROW1=126 ROW2=90 ROW3=54`). Memory after
`avail flush`, before against after: 0 for 14 demos; MUI-Demo with all eight
sub-windows 128 bytes on the first run after a cold boot and 0 on the second
(as Zune68k), Class3 48, AppWindow 64, Popup -24, Virtual -48 (MUI 3.8
loses 176-272 bytes per demo under the same test). No trap or alert.

## Which identity, which branch

`MUI5_TEST=0` changes only the reported numbers (`lib_Version` 19.81, built-in
classes 1.1); the code and memory are identical (832/334 KB above). Programs
written for MUI 4/5 refuse a library below 20, so the lean edition keeps 21.227.
mui38-dev is not leaner per window for structural reasons: of its 20 KB lead,
~17 KB is smart refresh (which MUI 3.8 itself uses). Its real lead is ~80 KB of
code, which on-demand class files can win back on main, and it lacks main's 141
later commits (speed work, String/List/menu/requester compatibility, IBrowse,
Voyager and YAM fixes). Base: main.

## Tools

- `tests/lean/scripts/`: `build.sh`, `measure.sh`, `libonly.sh`, `libsopen.sh`,
  `tally.sh`, `fullcheck.sh` (+ `leak.sh`, `subwin.sh`), `snoop.sh`, and the
  wrappers `m.sh` / `full.sh`. They drive `amiga-test` instances over wasabi;
  `ZWORK` (default `~/.cache/zune68-lean`) holds `demos/` (MUI 3.8's demo
  binaries), `logs/`, `res/`, `suite/`, and the `libs` / `HoldLib` helpers
  built from `tests/lean/libs.c` and `holdlib.c`. Pass FS-UAE options as
  `EXTRA="--config fast_memory=4096"` (no spaces inside a setting).
- `tests/lean/memtally/`: the allocation tally (`run`, `dump`, `guard`).
- `tools/lean/mtreport2.py DUMP LIB cats|files|big|N`, `tools/lean/classsizes.py
  BUILD`, `tools/lean/flexcat-sd.py`.
