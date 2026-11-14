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
  (2026-11-13, `rw-window`: by then it was a 1 KB pool puddle, held by every window that had
  been opened, for its cycle chain and gadget IDs; both are small blocks now, "Rewrite: Window"
  below.)

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

## Memory, 2026-11-05: notifications, image specs, four more class files

MUI-Demo cold, OS 3.1 + 4 MB fast, 68000, GCC 16.2, `m.sh` (two runs each
agree within 0.1 KB) and `tally.sh` (second MUI-Demo's own blocks):

| | library alone | first program | each further | tally | notifications |
|---|---:|---:|---:|---:|---:|
| lean c8c3a0d2 | 319.5 KB | 550.0 KB | 244.3 KB | 170.8 KB / 1,179 | 13.0 KB / 268 |
| + Notify (4b8072cc) | 319.6 KB | 545.8 KB | 240.0 KB | 166.4 KB / 1,180 | 8.6 KB / 268 |
| + image specs, class files (e2205d20) | 308.6 KB | 532.2 KB | 237.4 KB | | |
| MUI 3.8 (2026-11-04) | 238 KB | 460 KB | 211-219 KB | | |

- **Notify**: a node was 32 bytes + parameters, and an MUIV_EveryTime node
  kept the parameters twice so the trigger value could be put in. Now 24
  bytes (no pointers to its own parameters, flags packed), the message with
  the trigger value is built in a 32-byte buffer on the stack, and only a
  node with more than 7 parameters keeps a second copy. AllocMem with the
  size known at free. `check_notifications.py` covers 3, 7 and 9 parameters.
- **Image specs**: 205 StrDup copies of "6:N"/"0:N" strings per MUI-Demo,
  one per object background; the built-in ones are now shared constants.
- **Class files**: Register, Bitmap, Bodychunk, ChunkyImage (11.8 KB of
  code); the Pages demo (Register) and MUI-Demo's images load them.
- Where a further window's memory is now (tally): objects 100.4 KB / 340
  (Group 58 x 344 bytes, Text 72 x 226, Image 75 x 212, Rectangle 45 x 188,
  List 11 x ~766, Prop 21 x 300, Scrollbar 14 x 364, Listview 13 x 348;
  every object pays 12 + 28 + 136 bytes for BOOPSI, Notify and Area), lists
  20.5 KB (two 1 KB pool puddles in each of 7 filled lists), smart refresh
  backing store 17.1 KB, notifications 8.6 KB.
- perfbench in the same session: create/dispose -5%, the rest within 0.3%.
- Checks: `make check`, fullcheck and real.sh as on lean.

## Instance sizes against MUI 3.8, and Group and Area (2026-11-05)

`MUI_GetClass()` on both toolkits (a probe printing `cl_InstOffset +
cl_InstSize`) shows where a further window's memory was: MUI 3.8's second
MUI-Demo holds 138.9 KB (memtally: ten 8 KB object-pool puddles, 13 x 2 KB
list pools), lean's 166 KB. Own instance data, bytes:

| class | MUI 3.8 | lean before | lean now |
|---|---:|---:|---:|
| Area (every object) | 108 | 136 | 114 |
| Group (Zune's List, Listview, Scrollbar too) | 42 | 168 | 96 |
| Numeric | 60 | 98 | 98 |
| List | 390 | 420 | 420 (2026-11-13 `rw-list`: 324) |

- Group: the virtual-group state (event node, offsets, extents, the paint
  clip; 68 bytes) is a block made at OM_NEW for virtual groups only; the
  others read zeroes through `GV()`. Writes go through `data->virt`, so
  one outside a virtual-only path writes through NULL: the one in
  Group__MUIM_Draw's `finish:` did, overwrote the 68000 vectors at 32-39
  and every program then failed without a window or took the machine
  down. `make check` passed anyway (its emulator lets low memory be
  written); fullcheck caught it.
- Area: the selected background and MUIA_Timer are in the live block
  (made at the first setup), FixWidthTxt/FixHeightTxt/ContextMenu/
  PointerType in a block made when one is set, the disable count a WORD.
  Offsets up to 108 unchanged.
- MUI-Demo, OS 3.1 + 4 MB fast: first program 532.2 -> 518.9 KB, each
  further window 237.4 -> 223.0 KB (MUI 3.8: 211-219), library alone
  308.6 -> 309.8 KB (the accessors, +1.2 KB code).
- perfbench, both builds side by side: create/dispose +2.3%, the other
  batches +0.4 to +0.6%; "list active" moves by 3% between runs of the
  same build when five instances run at once, so read it from paired runs.
- Checks: `make check`, fullcheck, real.sh, listclip pixels and
  list_repaint_probe as on lean.

## opbench: every common operation against MUI 3.8 (2026-11-12, speed round 1)

`tests/native/opbench.c` (one binary for MUI 3.8 and OpenMUI, MUI 3.8's API only; clicks,
typing and the menu go through input.device and the program's own MUI loop) on a1200-31-mui38
in cycle-exact FS-UAE (`tests/lean/scripts/opbench.sh`: `uae_cpu_cycle_exact`,
`uae_blitter_cycle_exact`, `uae_cycle_exact`, warp mode; the EClock counts emulated time, so
the PC's load does not move the figures). 9 repetitions, median ms per operation, two instances
at once and then swapped; where the two differ by more than 1% both are given (MUI 3.8's
instances differ in a few operations, OpenMUI's do not). Emulator figures.

| operation (ms per operation) | MUI 3.8 | OpenMUI lean | OpenMUI speed1 | speed1 / MUI 3.8 |
|---|---:|---:|---:|---:|
| prefs+app (empty) | 89.7 / 93.7 | 47.4 | 47.2 / 48.5 | 0.50-0.54 |
| app+windows create | 235.2 / 239.4 | 254.4 | 248.2 / 257.7 | 1.04-1.10 |
| window open+close | 520.3 | 521.0 | 440.9 | 0.84 |
| full redraw | 150.7 / 140.5 | 149.8 | 149.8 | 0.99-1.07 |
| relayout | 207.3 / 197.7 | 260.1 | 195.2 | 0.94-0.99 |
| slider set | 4.04 / 4.09 | 2.91 | 2.92 | 0.71 |
| notify chain | 7.82 / 7.93 | 6.03 | 6.03 | 0.76 |
| cycle set | 2.44 | 4.58 / 4.43 | 2.79 | 1.14 |
| radio set | 15.6 | 19.6 | 9.74 | 0.62 |
| text set | 2.51 | 2.41 | 2.41 | 0.96 |
| list insert 1 | 10.3 | 33.7 | 9.11 | 0.88 |
| list insert 100 | 832.4 | 86.4 | 87.0 | 0.10 |
| list insert 1000 | 6654.4 | 475.7 | 475.9 | 0.07 |
| list active | 21.2 | 22.5 | 21.0 | 0.99 |
| list scroll line | 14.6 | 17.6 | 15.7 | 1.07 |
| list scroll page | 44.4 | 43.1 | 41.6 | 0.94 |
| list sort 1000 | 659.2 | 110.9 | 109.2 | 0.17 |
| button click | 22.3 / 22.6 | 21.0 | 21.0 | 0.93 |
| string type | 7.75 / 7.90 | 10.2 | 9.34 | 1.18-1.20 |
| string backspace | 7.66 / 7.79 | 9.99 | 9.13 | 1.17-1.19 |
| menu open+close | 13.2 | 20.0 | 13.1 | 1.00 |
| iconify | 83.6 | 79.9 | 79.5 / 81.0 | 0.94-0.97 |
| uniconify | 446.0 | 795.3 | 371.5 | 0.83 |
| register page | 53.8 / 49.5 | 59.0 / 57.7 | 53.4 / 54.8 | 0.99-1.11 |
| popup open+close | 120.0 | (popup on another screen) | 120.0 | 1.00 |
| window resize | 220.1 / 230.1 | 220.5 | 220.1 | 0.96-1.00 |

lean is `lean` at 146a3269, speed1 the branch's final build. What each fix did:
[README.md](README.md#speed-round-1-every-common-operation-timed-against-mui-38-2026-11-12-gate-3).

## opbench, speed round 2 (2026-11-12)

The same benchmark, MUI 3.8 and the branch `speed2` side by side and then swapped (sp-a, sp-b),
lean (`lean` at 1f1c4408, speed round 1) twice in the same session; 9 repetitions, median ms per
operation; two figures where the two runs differ by more than 1%. Emulator figures (FS-UAE
cycle-exact, a1200-31-mui38, OS 3.1, 68EC020).

| operation (ms per operation) | MUI 3.8 | OpenMUI lean | OpenMUI speed2 | speed2 / MUI 3.8 |
|---|---:|---:|---:|---:|
| prefs+app (empty) | 93.7 / 90.2 | 48.2 / 47.7 | 48.3 | 0.52-0.54 |
| app+windows create | 239.0 / 235.5 | 260.1 / 253.9 | 253.5 | 1.06-1.08 |
| window open+close | 521.2 | 440.0 | 441.0 | 0.85 |
| full redraw | 150.6 | 150.0 | 149.8 | 0.99 |
| relayout | 207.1 | 195.5 | 194.3 | 0.94 |
| slider set | 4.04 | 2.91 | 2.91 | 0.72 |
| notify chain | 7.82 | 6.03 | 6.02 | 0.77 |
| cycle set | 2.44 | 2.79 | 2.38 | 0.98 |
| radio set | 15.6 | 9.74 | 9.73 | 0.63 |
| text set | 2.50 | 2.41 | 2.42 | 0.97 |
| list insert 1 | 10.3 | 9.24 / 9.10 | 8.50 | 0.82 |
| list insert 100 | 831.3 | 86.9 | 85.3 | 0.10 |
| list insert 1000 | 6654.7 | 477.0 | 472.9 | 0.07 |
| list active | 21.1 | 21.3 / 21.1 | 19.3 | 0.91 |
| list scroll line | 14.6 | 15.7 | 14.4 | 0.98 |
| list scroll page | 44.4 | 41.7 | 40.0 | 0.90 |
| list sort 1000 | 658.1 | 108.6 / 110.3 | 110.1 | 0.17 |
| button click | 23.1 / 22.4 | 21.1 | 21.1 / 21.5 | 0.92-0.95 |
| string type | 7.46 / 7.84 | 9.36 | 7.77 | 0.99-1.04 |
| string backspace | 7.39 / 7.68 | 9.14 | 7.60 | 0.99-1.03 |
| menu open+close | 13.2 / 13.4 | 13.2 | 13.2 | 0.98-1.00 |
| iconify | 87.2 / 83.7 | 80.2 | 79.9 / 78.9 | 0.91-0.95 |
| uniconify | 448.9 | 376.2 / 372.2 | 373.9 | 0.83-0.84 |
| register page | 53.7 | 54.5 | 52.8 / 53.4 | 0.99 |
| popup open+close | 120.0 | 120.0 | 120.0 | 1.00 |
| window resize | 230.1 | 220.1 | 220.2 | 0.96 |

At least as fast as both MUI 3.8 instances: 23 of 26 (round 1: 19, lean before it: 12). Typing
and backspace are within MUI 3.8's own spread (its two instances 7.46 / 7.84 ms here, 7.59-7.90
in other runs of the day; OpenMUI's 7.77 on both); creating the application with both windows
1.06-1.08. MUI 3.8's full redraw and register page gave 140.9 and 49.5 ms on one instance in
round 1; this session both instances 150.6 and 53.7.

Memory (MUI-Demo cold, OS 3.1 + 4 MB fast, `m.sh`, FS-UAE, the same session, bytes):

| | first program | each further | library alone |
|---|---:|---:|---:|
| MUI 3.8 | 460,296 | 210,632-210,696 / 218,824-218,888 (it alternates) | 238,520 |
| lean (speed round 1) | 465,872 | 204,512-204,576 | 272,352 |
| speed2 | 460,208 | 204,512-204,576 | 266,688 |

Library code 241,848 -> 236,160 bytes (data 376 -> 396, BSS 2,940); Zmui45.mui 7,828 -> 14,836
bytes, Dtpic.mui 7,820 -> 8,056. What moved and why: [README.md](README.md#speed-round-2-typing-cycle-the-row-scroll-creation-and-the-first-program-2026-11-12-gate-3).

## opbench, speed round 3: MUI 5 and the OS calls (2026-11-12)

FS-UAE cycle-exact, median of 9, two instances then swapped; emulator figures. Against MUI 5
(a1200-32-mui5, OS 3.2.1, OpenMUI in MUI 5's place opened as version 21), ms per operation:

| operation | MUI 5 | OpenMUI speed3 (21) | ratio |
|---|---:|---:|---:|
| app+windows create | 577.9-586.8 | 235.9-249.8 | 0.40-0.43 |
| window open+close | 1026.6-1040.8 | 481.0-482.1 | 0.46-0.47 |
| full redraw | 206.3-206.6 | 156.8-156.9 | 0.76 |
| cycle set | 2.73-2.75 | 2.39-2.40 | 0.87 |
| radio set | 11.9 | 9.74 | 0.82 |
| list insert 1000 | 20253-20270 | 477.0-477.7 | 0.02 |
| button click | 21.2 | 17.7-17.8 | 0.83-0.84 |
| string type | 15.4 | 7.57-7.59 | 0.49 |
| uniconify | 1220.8-1236.4 | 422.0-422.4 | 0.34-0.35 |
| text shorten set (V20) | 4.68-4.69 | 3.40-3.45 | 0.73-0.74 |

All 27 faster (the whole table: README.md "Speed round 3"). Against MUI 3.8 (OS 3.1):
creation 236.4-241.0 / 233.8-238.3, button click 22.4 / 17.5-17.6, string type 7.82-7.97 /
7.37-7.38, slider set 4.04 / 2.73, list scroll line 14.6 / 13.3, register page 53.8-53.9 /
53.9-55.1 (MUI 3.8 / speed3).

OS calls per operation at top level (`opbench count`, tests/native/syscount.h), MUI 3.8 /
OpenMUI lean / speed3 / MUI 5: creation 2,417 / 4,146 / 1,534 / 8,595; radio set 29 / 146 / 28 /
329; button click 112 / 162 / 78 / 238; list scroll line 81 / 154 / 91 / 322. GetAttr per click
1 / 42 / 4 / 4.7; NextTagItem per creation 45 / 2,579 / 0 / (MUI 5 1,493 per redraw).

Memory (MUI-Demo cold, OS 3.1, `m.sh`, FS-UAE, the same session, bytes):

| | first program | each further | library alone |
|---|---:|---:|---:|
| MUI 3.8 | 460,296 | 210,632-210,696 / 218,888 | 238,520 |
| lean | 460,208 | 204,512 | 266,688 |
| speed3 | 459,992 | 204,512-204,576 | 266,472 |

Library code 236,160 -> 235,944 bytes.

## Rewrite: List (2026-11-13, branch `rw-list`)

README.md "Rewrite: List" has the steps. Figures (FS-UAE, OS 3.1, emulator):

| | lean (85a64a82) | rw-list |
|---|---:|---:|
| List's code (`classes/list.o`) | 24,716 | 23,876 |
| Zlist.mui code | 5,920 | 5,880 |
| library code (`size`) | 236,172 | 235,332 |
| library file, stripped | 252,656 | 251,912 |
| List instance data (MUI 3.8: 390) | 392 | 324 |
| a row (entry) | 24 + 4 per column, pooled, + a 4-byte pointer | 8, in one array |
| struct ColumnInfo | 64 | 32 |
| a format's blocks | 3-4 AllocVecs | 1 |
| MUI-Demo first program (MUI 3.8: 460,296) | 460,240 | 452,848 |
| MUI-Demo each further (MUI 3.8: 210,632-218,888) | 204,528-204,592 | 197,976-198,040 |

The per-program saving is mostly the pool puddles: every list with an entry held at least one
1 KB puddle for its rows; now only lists whose construct hook allocates from the pool (or a
program's MUIA_List_Pool) have one, and the rows are one AllocVec'd array (8 bytes per row,
doubled as it grows).

## Rewrite: Window (2026-11-13, branch `rw-window`)

README.md "Rewrite: Window" has the steps. Figures (FS-UAE, OS 3.1, emulator):

| | lean (4e8ac5b8) | rw-window |
|---|---:|---:|
| Window's code (`classes/window.o`) | 24,548 | 21,436 |
| library code (`size`) | 235,332 | 232,248 |
| library file, stripped | 251,912 | 248,804 |
| Zwintitle.mui / Zmui45.mui (files) | 6,448 / 15,252 | 7,780 / 16,156 |
| Window instance data (MUI 3.8: 698) | 494 | 480 |
| per opened window: cycle chain and gadget IDs | pool header + 1 KB puddle | 8-pointer block (doubled as needed) + 32-byte ID map |
| MUI-Demo first program (MUI 3.8: 460,296) | 452,848 | 448,488 |
| MUI-Demo each further (MUI 3.8: 210,632-218,888) | 198,040 | 196,704-196,768 |
| stackrun MUI-Demo / Layout / Virtual / Pages / Slidorama | 5,044 / 2,268 / 2,212 / 2,240 / 2,304 | 5,044 / 2,224 / 2,168 / 2,196 / 2,260 |

The first program's 4.4 KB: 3.1 KB of library code and 1.3 KB of data (the main window's pool);
each further MUI-Demo 1.3 KB (its main window; the eight sub-windows are not opened). Every
further opened window saves about 1 KB more.

## Prove it: drawing and memory (2026-11-13, branch `draw1`)

FS-UAE cycle-exact; opbench medians, MUI 3.8 / lean / draw1 (docs/lean/README.md "Prove it"):

| | MUI 3.8 | lean | draw1 |
|---|---:|---:|---:|
| A1200 window open+close | 520.4-535.7 | 440.1-441.1 | 420.1-421.7 |
| A1200 full redraw | 140.6-150.9 | 148.2-148.4 | 148.4-148.5 |
| A1200 relayout | 197.6-207.3 | 192.6-193.0 | 192.8-192.9 |
| A1200 window resize | 220.4-230.1 | 220.1 | 220.0-220.1 |
| A1200 register page | 53.6-53.8 | 53.4-54.5 | 45.4-45.6 |
| A1200 uniconify | 446.0-452.8 | 370.4-371.4 | 356.2-357.0 |
| A1200 popup open+close | 120.0 | 120.0 | 97.1-98.1 |
| A600 (68000, +4 MB fast) window open+close | 1762.5-1780.6 | 1536.1-1537.8 | 1473.2-1484.9 |
| A600 full redraw | 469.8-476.1 | 511.3-512.5 | 505.3-506.7 |
| A600 register page | 160.8 | 193.8-196.1 | 158.0-158.3 |
| A600 uniconify | 1534.2-1542.4 | 1367.9-1370.6 | 1290.8-1303.4 |
| Virtgroup 8-pixel scroll step, 80 buttons (sgstep.c) | 71.1 | 2,871.7 | 92.6 |

OS floor (no MUI, t/floor.c): window open 61-64 ms, close 53-57, menu strip 12-13 + 12-13;
ChangeWindowBox to IDCMP_NEWSIZE 218.7 ms. drawcost.c (opbench's screen, 68020): Text 9 chars
500 us, RectFill 1x20 173 us, Move+Draw 176, InstallClipRegion pair 2.0-2.1 ms, InitRastPort
68-95 us. Memory (MUI-Demo cold): first program 450,848-450,896 bytes (lean 448,488, MUI 3.8
460,296), each further 196,728-196,776.

## Rewrite: Group (2026-11-13, branch `rw-group`)

README.md "Rewrite: Group" has the steps. Figures (FS-UAE cycle-exact, OS 3.1, emulator; A600 =
a600-31 + 4 MB fast RAM, MUI 3.8 copied on):

| | MUI 3.8 | lean (65eed3a2) | rw-group |
|---|---:|---:|---:|
| Group's code (`classes/group.o`) | | 22,820 | 19,304 |
| library code (`size`) | | 234,428 | 231,344 |
| library file, stripped | | 251,088 | 248,012 |
| Group instance data | 42 | 68 | 36 |
| MUI-Demo first program | 460,296 | 450,848 | 444,592 |
| MUI-Demo each further | 210,632-218,888 | 196,776-196,840 | 193,608-193,672 |
| A1200 full redraw (ms) | 140.6-150.9 | 148.2-148.6 | 140.9-141.0 |
| A1200 app+windows create | 235.2-239.4 | 232.4-244.6 | 218.7-219.8 |
| A1200 relayout | 197.6-207.3 | 192.8-193.1 | 180.1-180.2 |
| A600 full redraw | 470.2-478.1 | 504.1-506.6 | 477.6-477.7 |
| A600 app+windows create | 916.8-941.2 | 935.4-960.1 | 897.0-946.3 |
| A600 relayout | 697.8-705.7 | 691.5-694.9 | 643.1-643.6 |
| stackrun MUI-Demo / Layout / Virtual / Pages / Slidorama | | 5,044 / 2,224 / 2,168 / 2,196 / 2,260 | 4,992 / 2,224 / 2,168 / 2,196 / 2,260 |

## Rewrite: Area (2026-11-13, branch `rw-area`)

README.md "Rewrite: Area" has the steps. Figures (FS-UAE cycle-exact, OS 3.1, emulator; A600 =
a600-31 + 4 MB fast RAM, MUI 3.8 copied on; MUI 3.8 on the A1200 from round draw1):

| | MUI 3.8 | lean (0b58e8c3) | rw-area |
|---|---:|---:|---:|
| Area's code (`classes/area.o`) | | 17,072 | 15,568 |
| library code (`size`) | | 231,312 | 229,896 |
| library file, stripped | | 247,980 | 246,504 |
| Area instance data | 108 | 108 | 108 |
| live block (input objects, with AllocVec's) | | 80 | 72 |
| MUI-Demo first program | 460,296 | 444,560-444,608 | 443,048 |
| MUI-Demo each further | 210,632-218,888 | 193,560-193,672 | 193,512-193,576 |
| A1200 full redraw (ms) | 140.6-150.9 | 140.4-141.1 | 133.7 |
| A1200 app+windows create | 235.2-239.4 | 216.4-227.7 | 225.0-225.4 |
| A1200 relayout | 197.6-207.3 | 180.2-180.6 | 173.4 |
| A600 full redraw | 470.2-476.7 | 475.0-477.9 | 444.7-444.9 |
| A600 app+windows create | 928.8-945.4 | 893.8-930.5 | 915.8-923.3 |
| A600 relayout | 697.7-704.8 | 639.6-644.0 | 613.4-614.3 |
| stackrun MUI-Demo / Layout / Virtual / Pages / Slidorama | | 4,992 / 2,224 / 2,168 / 2,196 / 2,260 | 4,976 / 2,224 / 2,168 (one run of four 2,204) / 2,196 / 2,260 |

The instance data was already MUI 3.8's 108 bytes (since 072a96c4; the table "Instance sizes
against MUI 3.8" above still shows the 114 of the step before).

## Rewrite: Application (2026-11-14, branch `rw-app`)

README.md "Rewrite: Application" has the steps and every operation. Figures (FS-UAE cycle-exact,
OS 3.1, emulator; A600 = a600-31 + 4 MB fast RAM, MUI 3.8 copied on; this session's instances):

| | MUI 3.8 | lean (43cfa4f3) | rw-app |
|---|---:|---:|---:|
| Application's code (`classes/application.o`) | | 16,120 | 11,484 (+ Zapp.mui 6,932 file, 3,824 code) |
| library code (`size`) | | 229,884 | 226,268 |
| library file, stripped | | 246,492 | 242,664 |
| Application instance data | | 370 | 370 |
| MUI-Demo first program | 460,296 | 443,040 | 439,760 |
| MUI-Demo each further | 210,632-218,888 | 193,512-193,576 | 193,584 |
| A1200 app+windows create (ms) | 235.5-236.3 | 215.2-232.7 | 183.5-189.4 |
| A1200 prefs+app (empty) | 92.3-93.1 | 53.5-62.3 | 15.9-16.2 |
| A1200 / A600 prefs+app, full global.prefs | | 321.4-328.0 / 1,371.9 | 57.4 / 237.4 |
| A600 app+windows create | 947.8 (928.8-945.4) | 900.0-930.1 | 758.4 |
| A600 prefs+app (empty) | 369.6 | 215.5-222.4 | 66.0-66.9 |
| A1200 / A600 full redraw | 140.4-150.7 / 472.2 | 133.4-133.8 / 443.2-445.8 | 133.7-133.8 / 444.6-444.8 |
| stackrun MUI-Demo / Layout / Virtual / Pages / Slidorama | | 4,976 / 2,224 / 2,168 / 2,196 / 2,260 | 4,924 / 2,224 / 2,168 / 2,196 / 2,260 |

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
- `tests/native/opbench.c` with `opbench.sh` (a run), `opprof.sh` (one operation under
  `tests/lean/pcsample.c`), `pcsample-full.py` (the profile with static functions, from the link
  map) and `opbench-compare.py` (runs side by side).
- `tests/native/alloccount.c`: exec allocations of an application made and disposed, counted
  with SetFunction (black box, any muimaster.library; `alloccount trace` lists the callers);
  `keyoffers.c`: which key handlers a key is offered to, in which order; `disabledget.c`:
  whether a window asks an event handler's object for MUIA_Disabled; `syscount.h` (in opbench:
  `count`, `time`, `who=NAME`) with `syscount-compare.py` and `syscount-who.py`; `pagepix.c` and
  `scrollpix.c`: the window's checksums after page switches and list scrolls.
- `tools/lean/mtreport2.py DUMP LIB cats|files|big|N`, `tools/lean/classsizes.py
  BUILD`, `tools/lean/flexcat-sd.py`.
