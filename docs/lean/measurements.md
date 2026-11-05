# Lean edition: where Zune68's memory goes (2026-11-04)

The results of steps 2-7 are in [The lean steps](#the-lean-steps-2026-11-04),
steps 8-13 (a compiler bug, Group, division, fonts, List, Dtpic) in [the
second session](#the-lean-steps-second-session-2026-11-04), and both in
[Final comparison](#final-comparison) below; step 1 (this report) follows
first.

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

## The lean steps (2026-11-04)

One row per step, each measured cold with the same method as above:
MUI-Demo, OS 3.1, A1200 68EC020, 2 MB chip + 4 MB fast (`a1200-31 --config
fast_memory=4096`), 68000 build (GCC 6.5, `-Os`), FS-UAE. "fullcheck" is
`fullcheck.sh` (19 MUI 3.8 demos + MUI-Demo's 8 sub-windows, Avail after
`avail flush` before/after each).

| step | commit | library file | library only | first MUI-Demo | each further | first window | fullcheck |
|---|---|---|---|---|---|---|---|
| 1 rexxsyslib v36 | be74ee7d | 429,472 | 535 KB | 832 KB | 334 KB | 2.0 s | all open, -48..+128 B |
| 2 libraries on first use | 294fa0af | 430,488 | 431 KB | 753 KB | 334 KB | 1.92 s | all open, 0..+128 B |
| 3 per-application/list memory | 7f31a15e | 430,800 | 432 KB | 720 KB | 301 KB | 1.9 s | all open, -48..+128 B |
| 4 Area nodes at first setup | 2475c0fd | 431,128 | 432 KB | 686 KB | 266 KB | 1.9 s | all open, -48..+128 B |
| 5 TheBar/YAM behaviour | f949dc3d | 431,680 | 433 KB | 686 KB | 266 KB | 1.9 s | all open, -104..+128 B |
| 6 classes as files | cdecd1a7 | 322,240 (+160 KB in 35 files) | 331 KB | 601 KB | 266 KB | 1.94 s | all open, -24..+104 B |
| 7 remaining per-object fat | - | (no change, see below) | | | | | |

- **Step 2**: at start only dos, utility, graphics, intuition, layers,
  gadtools, keymap and locale are opened; with MUI-Demo running asl,
  datatypes and mathieeedoubtrans are no longer in memory. diskfont stays
  (MUI-Demo's frame titles use helvetica/9, which is not in memory);
  rexxsyslib and commodities are also opened by other programs on the test
  machine.
- **Step 3**: Application data 6,348 -> 352 bytes.
- **Step 4**: Area data 238 -> 136 bytes (the fields MUI programs read at
  fixed offsets, up to mad_SelBack, are unchanged).
- **Step 5**: no memory change; TheBar demo2 now also opens its control
  window and demo3 its window (it stayed without one), YAM's error counter
  reads "Error 1/1" (was "OcError 1/1").
- **Step 6**: MUI-Demo loads five class files (Floattext, Dirlist,
  Volumelist, Gauge, Scale). `docs/lean/class-split.md`.

Memory tally (`tally.sh`, second MUI-Demo's own task) after step 6:
**192.3 KB / 1,488 blocks** (step 1: 258.8 KB / 1,639; Zune68k 137.3 KB):
objects 117.8 KB / 504, lists 20.5 KB / 97, smart refresh 17.2 KB,
notifications 16.3 KB / 340 blocks (was 16.7 KB / 529), text engine 8.6 KB,
windows 3.5 KB, Area live blocks 2.7 KB / 24 (only the objects of the one
open window).

**Step 7, not done:** what is left per object is small or not safe to
change. The largest is one Family object per Group (and per menu), 117
objects of 54 bytes (~7.5 KB per MUI-Demo); Group forwards its child-list
methods to it, so removing it is a Group rewrite. Text (50 bytes), String
(304) and List (416) carry Zune68's newer features (marking, Text_Data,
Textinput API, multi-line, column state); shrinking them saves well under
1 KB per MUI-Demo. Area's remaining fields up to offset 120 follow MUI's
layout for programs that read them.

## The lean steps, second session (2026-11-04)

Same method (MUI-Demo cold, OS 3.1, A1200 68EC020, 2 MB chip + 4 MB fast,
68000 build with bebbo's GCC 6.5, FS-UAE); each build measured with
`measure.sh` (twice), `libonly.sh`, `fullcheck.sh` and `bench.sh`
(perfbench), the bigger ones with `real.sh` as well.

| step | commit | library file | library only | first MUI-Demo | each further | first window | fullcheck |
|---|---|---|---|---|---|---|---|
| 6 classes as files (start) | cdecd1a7 | 322,240 | 330.5 KB | 601 KB | 266 KB | 1.94 s | all open, -24..+104 B |
| 8 GCC 6.5 auto-increment pass off | cd4f882e | 322,748 | 331.0 KB | 602.0 KB | 266.4 KB | 1.94 s | all open, 0..+128 B |
| 9 Group without Family object | 3bde59da | 323,004 | 331.3 KB | 596.6 KB | 260.9 KB | 1.92 s | all open, 0..+128 B |
| 10 own 64/32-bit division | fbba14ed | 319,468 | 328.0 KB | 593.3 KB | 260.9 KB | 1.90-1.92 s | all open, 0..+128 B |
| 11 screen font by default | 6e32efa3 | 319,440 | 327.9 KB | 575.3 KB | 260.8 KB | 1.80-1.87 s | all open, -24..+128 B |
| 12 List's horizontal scroller at setup | 140d5b1f | 319,448 | 327.9 KB | 557.9 KB | 243.4 KB | 1.64-1.72 s | all open, -48..+200 B (+128, 0, 0 in three runs) |
| 13 Dtpic as a class file | 08e1f52e | 314,512 (+163 KB in 36 files) | 323.2 KB | 553.4 KB | 243.4 KB | 1.72-1.76 s | all open, 0..+128 B |

(First-window times wander by ~0.1 s between runs when other emulators run
on the PC; `wintimes.sh` below is the better comparison.)

**perfbench**, `bench.sh` (15 repetitions, median, ms per batch, the same
68000 executable on all; MUI 3.8 measured in the same session):

| batch | MUI 3.8 | lean, step 6 | lean, step 13 |
|---|---:|---:|---:|
| create/dispose tree | 173.5 | 129.6 | 109.0 |
| open/close window | 480.2 | 417.5 | 399.4 |
| redraw x10 | 1292.7 | 1250.7 | 1237.4 |
| relayout x5 | 940.0 | 861.6 | 851.1 |
| insert 200 | 1500.8 | 952.2 | 941.9 |
| list active x50 | 1062.2 | 1142.3 | 1127.1 |
| notify x100 | 784.3 | **31,344.6** | 601.6 |
| text set x100 | 250.5 | 239.9 | 240.1 |

- **The 31 s notification chain** was not Zune's code: bebbo's GCC 6.5
  pass `-fbbb=i` compiled `data->min != oldmin` in Numeric's OM_SET as a
  compare with the value (it turned `(a2)` into `(a2)+` and left the later
  offsets), so every set of a slider relaid out its parent group. A slider
  set took 23.8 ms (MUI 3.8: 4.0 ms), now 2.75 ms. Zune68 main built with
  GCC 6.5 has it too. GCC 16.2 (the release builds) also has `-fbbb`, but
  there the "i" pass changes no code (see [compiler.md](compiler.md)).
- "list active" is the one batch slower than MUI 3.8 (+6%), on Zune68
  main too (1137 ms); not investigated.

**Window times** (`wintimes.sh`: three MUI-Demos started one after the
other, from `run` to its window, wall clock):

| | 1st (cold) | 2nd | 3rd |
|---|---|---|---|
| MUI 3.8 | 1.48-1.56 s | 1.09-1.11 s | 1.21-1.27 s |
| lean, step 8 | 1.90 s | 1.69 s | 1.75 s |
| lean, step 13 | 1.72 s | 1.47 s | 1.37 s |

Loading is not where the lean edition loses: a small program (`otime`,
Application + Window + a few objects, EClock) takes OpenLibrary 57 ms cold
(MUI 3.8: 209 ms, it loads muigfx, muilowlevel and commodities), the
application 65 ms (66), the window 176 ms (171); LoadSeg of the library
file alone is 49 ms (MUI 3.8: 22 ms). The difference is in what MUI-Demo
creates and opens; making the 13 unused horizontal list scrollers lazily
took 0.2 s off. A PC-sampling profiler (a pri-60 task reading the saved
PC of the task at the head of TaskReady every 1 ms) was not usable: in
FS-UAE it got one sample per 5-45 ms and mostly saw tasks just woken.

**Memory tally** (second MUI-Demo's own task, `tally.sh`): 192.3 KB /
1,488 blocks (step 6) -> 186.9 KB / 1,371 (step 9: 116 Family objects
fewer, objects 388) -> 170.0 KB / 1,180 (step 12: objects 340 / 99.5 KB,
lists 20.5 KB, backing store 17.1 KB, notifications 13.1 KB / 268, text
engine 5.9 KB, windows 5.6 KB). Instance sizes now: Area 136, Group 160
(with its child list), Text 50, Image 36, List 416, Window 726.

**Code size, what was tried** (68000, text of the linked library):

| build | text | |
|---|---:|---|
| step 8 | 301,672 | |
| `-fomit-frame-pointer` | -2,696 | link/unlk in 1,033 functions; a5 free |
| `-fno-partial-inlining`, `-fno-ipa-cp-clone`, `-fno-reorder-blocks`, `-fno-tree-switch-conversion` | -100..0 | |
| `-fno-inline-small-functions`, `-fno-jump-tables` | +370, +660 | |
| no `-falign-*` | -8 | `-Os` ignores them |
| `LTO=1` (GCC 6.5) | -3,624 | class files then do not link: no `nm` plugin for the class table, and `-ffat-lto-objects` makes this linker write no file at all (exit 0) |
| `-mregparm=2` | - | does not build |
| libgcc 64-bit division | -3,332 | done (step 10) |

The rest is Zune's own code (per object file: List 29.6 KB, Window 29.3,
String 21.2, Group 20.4, Area 15.7, Application 15.0, frame 11.3, text
engine 9.4, Configdata 8.6, vector images 8.2, context menus 7.7,
datatype pictures 7.1).

## Real programs (OS 3.1 + 4 MB fast + 68882, `real.sh`)

Third-party binaries (`~/.cache/zune68k-real/`): NList 0.128, TextEditor
15.56, TheBar 26.22, BetterString 11.36, YAM 2.5. Same results for Zune68
main (step 1) and every lean step unless noted:

| program | result |
|---|---|
| NList-Demo (two windows) | works; 2,000-2,040 bytes more in use after the second run, on main too |
| NListtree-Demo | no window, keeps waiting, **on main too** (not a lean regression; Zune68k opened it) |
| TextEditor-Demo | works, 0 lost |
| TheBar demo1-10 | all work from step 5 on (demo3 had no window and demo2 no control window before); 0 lost (demo4 -240 B, main -3,104 B) |
| YAM 2.5 | charset and AmiSSL requesters, error window, update window and main window open; toolbar labels drawn, disabled buttons embossed, not ghosted or black; "Error 1/1" (main: "OcError 1/1") |

Voyager and IBrowse are not on these test machines; the Area layout they
read (offsets up to 104) is unchanged.

## Final comparison

MUI-Demo, cold, same method; MUI 3.8 measured again in the same session
(`a1200-31-mui38`, 4 MB fast).

OS 3.1, A1200 68EC020, 2 MB chip + 4 MB fast:

| | library file | library only | first MUI-Demo | each further | first window |
|---|---|---|---|---|---|
| MUI 3.8 | 166 KB (+271 KB classes) | 238 KB | 460 KB | 211-219 KB | 1.48-1.5 s |
| Zune68k | 237 KB (+118 KB .mui) | 257 KB | 460-471 KB | 210-214 KB | 1.9-2.1 s |
| Zune68 main, 68000 | 429,472 | 535 KB | 832 KB | 334 KB | 2.0 s |
| lean, 68000, step 6 | 322,240 (+160 KB in 35 files) | 331 KB | 601 KB | 266 KB | 1.94 s |
| **lean, 68000, step 13** | 314,512 (+163 KB in 36 files) | **323 KB** | **553 KB** | **243 KB** | 1.72-1.76 s |
| Zune68 main, 68020 | 419,812 | 527 KB | 825 KB | 334 KB | 1.9 s |
| lean, 68020, step 6 | 314,372 (+ files) | 324 KB | 595 KB | 266 KB | 1.94 s |
| **lean, 68020, step 13** | 309,656 (+ files) | **320 KB** | **549 KB** | **243 KB** | 1.7 s |

OS 3.2.1, A1200 68EC020, 2 MB chip + 8 MB fast, 68000 build: main 444 /
739 / 337 KB, **lean 335 / 581 / 271 KB** (library only / first / each
further), first window 2.0 s both.

A600 68000 7 MHz, 3.1, 1 MB chip + 4 MB fast, 68000 build: main 832 / 334
KB, **lean 601 / 266 KB**, first window 6.5-7.8 s -> 6.6-6.7 s. (The
library-only figure of `libonly.sh` is not valid on this machine: 34 KB
for both.)

Against MUI 3.8 the lean edition (step 6) was still +93 KB with the
library alone (its code: 322 KB against 3.8's 166 KB), +141 KB for the
first program and +50 KB for each further one (of which 17 KB is smart
refresh backing store that 3.8 also has, but its demo windows are
smaller; and ~28 KB more in objects: 504 objects of 234 bytes on
average). After step 13: +85 KB library alone (code 315 KB), +93 KB first
program, +24-32 KB each further one (17 KB of it backing store), first
window +0.25 s.

## perfbench: main, lean and MUI 3.8 in one session (2026-11-05)

Unchanged main (origin/main 3b9299d5, rexxsyslib 36 for OS 3.1) against lean
4f22a12f, both built with GCC 16.2 for 68000 and 68020 (`bld.sh` flags as
in `build.sh`), and MUI 3.8, all with `bench.sh` on OS 3.1 + 4 MB fast,
the same 68000 perfbench, two cold boots each (all five instances in
parallel per round). ms per batch, mean of the two boots' medians (the two
boots differ by under 1%):

| batch | MUI 3.8 | main 68000 | lean 68000 | lean vs 3.8 | lean vs main (68000 / 68020) |
|---|---:|---:|---:|---:|---:|
| create/dispose tree | 171.4 | 131.3 | 107.0 | -37.5% | -18.4% / -19.2% |
| open/close window | 478.6 | 400.0 | 400.0 | -16.4% | 0 / -0.1% |
| redraw x10 | 1292.1 | 1238.8 | 1226.5 | -5.1% | -1.0% / -0.9% |
| relayout x5 | 939.6 | 853.9 | 845.9 | -10.0% | -0.9% / -0.9% |
| insert 200 | 1500.9 | 937.5 | 904.1 | -39.8% | -3.6% / -3.4% |
| list active x50 | 1063.3 | 1134.1 | 1129.4 | +6.2% | -0.4% / +0.2% |
| notify x100 | 783.8 | 597.2 | 601.0 | -23.3% | +0.6% / +0.6% |
| text set x100 | 250.7 | 239.4 | 240.5 | -4.1% | +0.5% / +0.5% |

- The lean steps win on creating objects (-18%) and inserting list entries
  (-3.5%); everything else is within 1% of main.
- Against MUI 3.8, main is already -16% on open/close and -24% on
  notifications here, where `tests/native/PERFORMANCE.md` (WinUAE, OS
  3.2.3, 68020, GCC 13.4) has 0% and -6%: the gap to that table is the
  machine and OS, not the lean edition.
- "list active" is 6-7% slower than MUI 3.8 on both main and lean here,
  but 8% faster in PERFORMANCE.md: something in this setup (OS 3.1's
  graphics or FS-UAE), not investigated.
- 68020 builds are 0-2% faster than 68000 ones in the same batches.

## List selection: the clip (2026-11-05)

Why "list active" is slower than MUI 3.8 on OS 3.1 (`perfbench` split into
parts, OS calls counted with 12-byte `addq.l #1,count; jmp old` stubs,
InstallClipRegion callers found by a frame-pointer walk; all FS-UAE, OS 3.1
+ 4 MB fast, 68000 builds):

| part | MUI 3.8 | lean | lean, no clip at all (test only) |
|---|---:|---:|---:|
| list active x50 (perfbench) | 1063 ms | 1128 ms | 1051 ms |
| cursor moves inside the view x50 | 208 ms | 278 ms | 192 ms |
| cursor moves that scroll by 7 rows x24 | 507 ms | 598 ms | 562 ms |

- Every List draw installed the entry area's clip: MUI_AddClipping, two
  InstallClipRegion calls, 13 AllocMem/FreeMem pairs. On OS 3.1 that is
  about 1.5 ms, more than repainting the two cursor rows. MUI 3.8 does not
  clip a cursor move (no InstallClipRegion at all) and fits the text with
  TextFit instead. Stefan's 3.2.3 measurement does not show this, so its
  layers are presumably cheaper here.
- Fix (`List_NeedClip`): a full repaint or a scroll still clips at once; a
  cursor move clips only when a row is cut off at the bottom, has object
  cells or formatted text, or its plain text leaves the area. Installing the
  clip late in a full repaint was 5% slower: InstallClipRegion then waits
  for the blitter to finish the queued rows.
- Result, same session: cursor moves inside the view 288 -> 194 ms
  (MUI 3.8: 208), list active x50 1142 -> 1119 ms (MUI 3.8: 1063), full
  list redraw and insert within 1%. `tests/lean/listclip.c` (row checksums
  of six lists with too-wide, styled, columned and cut-off rows through
  cursor moves, redraw and relayout) gives identical output with and
  without the change; `list_repaint_probe` passes (189 comparisons).
- Scroller (`Scrollbar_PropOnly`): each scroll step made 7 SetAttrsA
  calls (MUI 3.8: 2). Four of them set MUIA_Prop_First: on the Scrollbar,
  then Group forwarded it to the prop and to both arrow buttons, which
  ignore it. Scrollbar now hands a tag list of only Prop_First/Entries/
  Visible/NoNotify to the prop alone (the Scrollbar's own notifications
  still see it). Per step 7 -> 5 calls.
- Both changes, against lean b95f0a40 in one session: list active x50
  1143 -> 1108 ms (MUI 3.8: 1063, so +4% left), cursor moves inside the
  view 287 -> 194 ms, scrolling moves x24 606 -> 595 ms; the other perfbench
  batches within 0.3%, the list-only redraw +1% (474 -> 479 ms). `make
  check` passes; fullcheck (19 demos + 8 MUI-Demo windows) and `real.sh`
  (with `REALDIR=DH0:Real`) give the same results as lean.
- What is left is in the scrolling moves: the scroll still needs the clip
  because it repaints the cut-off last row (MUI 3.9 behaviour, see
  `listvisible.c`), and List sets MUIA_List_First and
  MUIA_List_VertProp_First as two separate calls. Intuition's own
  scroller refresh adds two InstallClipRegion calls per step on both
  toolkits.
- `make check` needs the toolchain's Python, an unstripped build and the
  toolchain's FlexCat first on PATH: `PATH=~/opt/amiga-gcc16/bin:$PATH make
  check PYTHON=~/opt/amiga-gcc16/libexec/amitools/bin/python` (the system
  machine68k has no `Machine.from_name`; the FlexCat stand-in cannot make
  catalogs).

## Tools

- `tests/lean/scripts/`: `build.sh`, `measure.sh`, `libonly.sh`, `libsopen.sh`,
  `tally.sh`, `fullcheck.sh` (+ `leak.sh`, `subwin.sh`), `snoop.sh`, the
  wrappers `m.sh` / `full.sh`, and `real.sh` (+ `realinstall.sh`,
  `realinstall2.sh`, `realrun.sh`, `putdir.sh`) for the third-party
  programs, `bench.sh` (perfbench on an instance) and `wintimes.sh` (three
  MUI-Demo starts, time to each window). `putlib.sh` also installs the class files (`Zune/` next to the
  library) to `LIBS:Zune/`. They drive `amiga-test` instances over wasabi;
  `ZWORK` (default `~/.cache/zune68-lean`) holds `demos/` (MUI 3.8's demo
  binaries), `logs/`, `res/`, `suite/`, and the `libs` / `HoldLib` helpers
  built from `tests/lean/libs.c` and `holdlib.c`. Pass FS-UAE options as
  `EXTRA="--config fast_memory=4096"` (no spaces inside a setting).
- `tests/lean/memtally/`: the allocation tally (`run`, `dump`, `guard`).
- `tools/lean/mtreport2.py DUMP LIB cats|files|big|N`, `tools/lean/classsizes.py
  BUILD`, `tools/lean/flexcat-sd.py`.
