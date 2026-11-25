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

## Rewrite: String (2026-11-17, branch `rw-string`)

README.md "Rewrite: String" has the steps and where a key's time goes. Figures (FS-UAE
cycle-exact, OS 3.1, emulator; A600 = a600-31 + 4 MB fast RAM, MUI 3.8 copied on; this session's
instances, MUI 3.8 also the Application round's):

| | MUI 3.8 | lean (3b4eee2c) | rw-string |
|---|---:|---:|---:|
| String's code (`classes/string.o`) | | 15,896 | 13,916 (+ Zstring.mui 3,304 file, 1,860 code; Zmui45.mui +984) |
| library code (`size`) | | 226,268 | 223,820 |
| library file, stripped | | 242,664 | 239,888 |
| String instance data | 88 | 308 | 96 |
| MUI-Demo first program | 460,296 | 439,760 | 436,936-436,984 |
| MUI-Demo each further | 210,632-218,888 | 193,520-193,584 | 193,376 |
| A1200 string type / backspace (ms) | 7.49-7.88 / 7.45-7.82 | 7.33-7.38 / 7.19-7.24 | 6.66-6.68 / 6.55-6.61 |
| A600 string type / backspace | 30.7-31.45 / 30.4-31.22 | 29.13-29.31 / 28.58-28.93 | 26.17-26.39 / 25.79-26.16 |
| MUI 5 identity (v21, a1200-32-mui5) type / backspace | | 7.53-7.58 / 7.45-7.46 | 7.44-7.50 / 7.40-7.41 |
| stackrun MUI-Demo / Layout / Virtual / Pages / Slidorama | | 4,924 / 2,224 / 2,168 / 2,196 / 2,260 | the same |

## Rewrite: text engine, images and frames (2026-11-17, branch `rw-draw`)

README.md "Rewrite: text engine, images and frames" has the steps. Figures (FS-UAE cycle-exact,
OS 3.1, emulator; A600 = a600-31 + 4 MB fast RAM, MUI 3.8 copied on; three instances of each
library in this session, MUI 3.8 the Application round's):

| | MUI 3.8 | lean (4fbc6aed) | rw-draw (fff0167b) |
|---|---:|---:|---:|
| textengine.o / imspec.o / imspec_vector.o / frame.o | | 8,180 / 4,588 / 8,184 / 6,796 (+1,816 bss) | 6,052 / 4,484 / 6,796 / 3,528 (+388 bss) |
| library code (`size`) / bss | | 223,820 / 3,364 | 216,648 / 1,936 |
| library file, stripped | | 239,888 | 232,224 |
| library alone in memory (`libonly`) | | 254,664 | 246,048 |
| Zframes.mui / Zmui45.mui (file) | | 9,208 / 17,840 | 9,320 / 20,028 |
| MUI-Demo first program | 460,296 | 436,936-436,984 | 428,344-428,368 |
| MUI-Demo each further | 210,632-218,888 | 193,376 | 193,312-193,376 |
| A1200 full redraw / text set (ms) | 140.4-150.7 / 2.50-2.51 | 133.6-133.7 / 2.390-2.393 | 132.9 / 2.393-2.402 |
| A1200 open+close / register page | 520.3-522.7 / 53.7-53.8 | 398.7-402.6 / 42.5-42.6 | 397.6-398.6 / 42.5 |
| A1200 cycle set / radio set | 2.44 / 15.6 | 2.02 / 8.50-8.51 | 2.01-2.02 / 8.26-8.27 |
| A600 full redraw / text set | 472.2 / 9.72 | 442.5-446.0 / 9.42-9.46 | 441.9-442.4 / 9.42-9.45 |
| A600 open+close / register page | 1,779.4 / 164.1 | 1,401.9-1,419.0 / 145.3-146.2 | 1,384.4-1,396.4 / 145.0-145.7 |
| A600 cycle set / radio set | 9.44 / 60.7 | 7.87-7.96 / 32.9-33.0 | 7.86-7.94 / 32.4 |
| stackrun MUI-Demo / Layout / Virtual / Pages / Slidorama | | 4,924 / 2,224 / 2,168 / 2,196 / 2,260 | the same |

## Measurement of 2026-11-19: opbench against MUI 3.8 and MUI 5, memory (branch `meas`, build of that day)

Emulator figures (FS-UAE, cycle-exact, warp mode; the EClock counts emulated time), the library built
from branch `lean` (68000 build, GCC 16.2), `tests/lean/scripts/opbench.sh`, medians of 9 repetitions
in ms per operation, two instances of each library, the second round with the instances swapped. A1200
= a1200-31-mui38 (68EC020, OS 3.1, 4 MB fast); A600 = a600-31 (68000, OS 3.1) with `fast_memory = 4096`
and MUI 3.8's `MUI:` drawer and startup assigns copied on; MUI 5 = a1200-32-mui5 (OS 3.2.1, MUI 5
21.227) against OpenMUI in its place (`MUI:Libs` moved aside, `opbench v21`, MUI 5 identity). Last
column: mean of the two OpenMUI instances over the mean of the two reference instances. "text shorten
set" (V20) runs only as version 21; MUI 5's two instances come from a second pair of runs.

### A1200 (68EC020): OpenMUI against MUI 3.8

| operation | MUI 3.8 inst 1 | MUI 3.8 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | OpenMUI / MUI 3.8 |
|---|---:|---:|---:|---:|---:|
| prefs+app (empty) | 92.12 | 90.36 | 15.89 | 15.98 | 0.17 |
| app+windows create | 235.15 | 234.58 | 189.52 | 189.24 | 0.81 |
| window open+close | 520.25 | 521.47 | 399.20 | 400.95 | 0.77 |
| full redraw | 150.61 | 140.31 | 132.85 | 132.91 | 0.91 |
| relayout | 207.00 | 197.29 | 172.59 | 172.45 | 0.85 |
| slider set | 4.04 | 4.10 | 2.71 | 2.71 | 0.67 |
| notify chain | 7.82 | 7.93 | 5.63 | 5.63 | 0.72 |
| cycle set | 2.44 | 2.44 | 2.02 | 2.01 | 0.83 |
| radio set | 15.56 | 15.56 | 8.27 | 8.27 | 0.53 |
| text set | 2.50 | 2.51 | 2.39 | 2.39 | 0.95 |
| list insert 1 | 10.34 | 10.31 | 8.19 | 8.18 | 0.79 |
| list insert 100 | 832.41 | 831.15 | 60.99 | 61.63 | 0.07 |
| list insert 1000 | 6,648.4 | 6,650.8 | 248.76 | 248.62 | 0.04 |
| list active | 21.13 | 21.13 | 18.80 | 18.81 | 0.89 |
| list scroll line | 14.63 | 14.59 | 13.26 | 13.24 | 0.91 |
| list scroll page | 44.40 | 44.41 | 39.18 | 39.17 | 0.88 |
| list sort 1000 | 658.04 | 657.98 | 94.03 | 95.01 | 0.14 |
| button click | 22.56 | 22.43 | 16.57 | 16.88 | 0.74 |
| string type | 7.85 | 7.82 | 6.66 | 6.66 | 0.85 |
| string backspace | 7.72 | 7.72 | 6.54 | 6.61 | 0.85 |
| menu open+close | 13.19 | 13.22 | 13.15 | 13.13 | 0.99 |
| iconify | 84.12 | 84.39 | 77.81 | 78.42 | 0.93 |
| uniconify | 448.75 | 448.52 | 336.96 | 338.02 | 0.75 |
| register page | 53.67 | 53.73 | 42.61 | 42.58 | 0.79 |
| popup open+close | 120.04 | 120.01 | 95.71 | 95.67 | 0.80 |
| window resize | 230.10 | 230.10 | 220.08 | 220.09 | 0.96 |


All 26 at 0.99 or below (0.04 to 0.99).

### A600 (68000, 4 MB fast): OpenMUI against MUI 3.8

| operation | MUI 3.8 inst 1 | MUI 3.8 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | OpenMUI / MUI 3.8 |
|---|---:|---:|---:|---:|---:|
| prefs+app (empty) | 354.21 | 353.05 | 65.82 | 67.02 | 0.19 |
| app+windows create | 931.90 | 930.86 | 752.81 | 763.75 | 0.81 |
| window open+close | 1,760.8 | 1,754.0 | 1,380.8 | 1,390.4 | 0.79 |
| full redraw | 470.06 | 474.21 | 441.65 | 441.10 | 0.93 |
| relayout | 697.82 | 701.59 | 607.66 | 607.70 | 0.87 |
| slider set | 15.70 | 15.12 | 10.09 | 10.10 | 0.66 |
| notify chain | 30.58 | 29.37 | 20.90 | 20.85 | 0.70 |
| cycle set | 9.34 | 9.32 | 7.84 | 7.81 | 0.84 |
| radio set | 60.41 | 60.39 | 32.32 | 32.30 | 0.53 |
| text set | 9.68 | 9.66 | 9.43 | 9.42 | 0.97 |
| list insert 1 | 32.40 | 32.38 | 32.95 | 32.53 | 1.01 |
| list insert 100 | 2,738.9 | 2,743.2 | 230.21 | 225.88 | 0.08 |
| list insert 1000 | 23,913.5 | 23,912.2 | 1,016.9 | 1,007.9 | 0.04 |
| list active | 64.84 | 64.83 | 61.37 | 60.59 | 0.94 |
| list scroll line | 39.88 | 39.97 | 37.38 | 37.03 | 0.93 |
| list scroll page | 153.59 | 153.54 | 134.67 | 133.76 | 0.87 |
| list sort 1000 | 2,490.0 | 2,487.5 | 378.92 | 386.27 | 0.15 |
| button click | 92.33 | 91.50 | 68.64 | 68.64 | 0.75 |
| string type | 31.37 | 31.66 | 26.27 | 26.22 | 0.83 |
| string backspace | 31.09 | 31.28 | 25.82 | 25.83 | 0.83 |
| menu open+close | 46.49 | 46.79 | 47.48 | 47.23 | 1.02 |
| iconify | 295.81 | 301.69 | 230.63 | 232.24 | 0.77 |
| uniconify | 1,529.6 | 1,535.0 | 1,247.8 | 1,247.9 | 0.81 |
| register page | 160.09 | 160.14 | 144.86 | 144.96 | 0.91 |
| popup open+close | 404.47 | 407.75 | 338.62 | 341.63 | 0.84 |
| window resize | 620.21 | 620.20 | 521.34 | 528.22 | 0.85 |


24 of 26 below 0.98 (0.04 to 0.97); list insert 1 (1.01) and menu open+close (1.02) level.

### OS 3.2.1: OpenMUI (MUI 5 identity) against MUI 5 21.227

| operation | MUI 5 inst 1 | MUI 5 inst 2 | OpenMUI v21 inst 1 | OpenMUI v21 inst 2 | OpenMUI v21 / MUI 5 |
|---|---:|---:|---:|---:|---:|
| prefs+app (empty) | 299.74 | 300.65 | 17.29 | 17.77 | 0.06 |
| app+windows create | 577.34 | 577.02 | 201.30 | 202.88 | 0.35 |
| window open+close | 1,028.1 | 1,030.2 | 442.20 | 441.51 | 0.43 |
| full redraw | 206.01 | 205.99 | 139.03 | 138.99 | 0.67 |
| relayout | 316.84 | 316.56 | 181.52 | 181.57 | 0.57 |
| slider set | 6.75 | 6.74 | 3.02 | 3.02 | 0.45 |
| notify chain | 12.75 | 12.76 | 6.22 | 6.22 | 0.49 |
| cycle set | 2.72 | 2.73 | 2.05 | 2.05 | 0.75 |
| radio set | 11.86 | 11.87 | 8.39 | 8.39 | 0.71 |
| text set | 2.25 | 2.25 | 1.65 | 1.66 | 0.74 |
| list insert 1 | 22.80 | 22.81 | 9.04 | 9.04 | 0.40 |
| list insert 100 | 2,033.8 | 2,033.2 | 61.37 | 61.43 | 0.03 |
| list insert 1000 | 20,206.4 | 20,201.1 | 248.69 | 249.08 | 0.01 |
| list active | 39.08 | 39.09 | 18.43 | 18.58 | 0.47 |
| list scroll line | 21.12 | 21.09 | 14.56 | 14.57 | 0.69 |
| list scroll page | 56.81 | 56.82 | 39.75 | 39.73 | 0.70 |
| list sort 1000 | 2,306.3 | 2,305.5 | 137.42 | 137.72 | 0.06 |
| button click | 21.20 | 20.76 | 16.86 | 16.68 | 0.80 |
| string type | 15.36 | 14.87 | 7.48 | 7.48 | 0.49 |
| string backspace | 14.77 | 14.32 | 7.41 | 7.41 | 0.51 |
| menu open+close | 26.22 | 22.54 | 19.98 | 19.99 | 0.82 |
| iconify | 198.11 | 197.67 | 98.64 | 97.97 | 0.50 |
| uniconify | 1,214.6 | 1,211.8 | 372.06 | 372.54 | 0.31 |
| register page | 88.54 | 88.48 | 44.91 | 44.91 | 0.51 |
| popup open+close | 527.00 | 538.10 | 104.09 | 104.06 | 0.20 |
| window resize | 290.25 | 290.10 | 170.06 | 169.94 | 0.59 |
| text shorten set | 4.68 | 4.69 | 3.47 | 3.43 | 0.74 |


All 27 below 0.83 (0.01 to 0.82).

### Memory (MUI-Demo cold, `measure.sh`, `libonly.sh`, bytes)

| | first program | each further | library alone | first window |
|---|---:|---:|---:|---:|
| OpenMUI, OS 3.1 | 428,152-428,176 | 193,032-193,120 | 246,088 | 1.30-1.32 s |
| MUI 3.8, OS 3.1 | 460,296 | 210,632-210,696 / 218,888 | 238,520 | 1.52-1.62 s |
| OpenMUI, MUI 5 identity, OS 3.2.1 | 434,840 | 201,704-201,744 | 249,968 | 1.68-1.70 s |
| MUI 5 21.227, OS 3.2.1 | 1,293,680 | 427,104-435,480 | 581,024 | 4.28-4.40 s |

Files on disk, stripped (`m68k-amigaos-strip`): OpenMUI muimaster.library 232,200 bytes (254,124 unstripped) and 54 class
files in `LIBS:OpenMUI` 289,948; MUI 3.8 166,412 and 36 files in `MUI:Libs/mui` 271,208; MUI 5 565,488
and 80 files 2,053,912. The identity setting of the MUI 5 row: `ENVARC:OpenMUI/BehaveAs` = `MUI5`.

### Test count

`tests/lean/real/programs.sh`: 185 `P` lines, 3 IDs listed twice (datestring-demo, envelope-demo,
tron-demo): 182 distinct entries, groups mui3 100, mui5 24, mcc 58 (61 lines). Earlier figures of 184
(100 + 24 + 60), 181 and 132 + 60 do not match the table; 182 is used in the documentation.

## Measurement of 2026-11-23: OpenMUI 1.0 against MUI 3.8, MUI 5 and Zune68 Beta 5

Emulator figures (FS-UAE, cycle-exact, warp mode; the EClock counts emulated time), `opbench.sh`
as on 2026-11-19, medians of 9 repetitions in ms per operation, two instances of each library; the
instances of a round ran concurrently and all libraries were measured in the same session. OpenMUI = the
library of release 1.0 (`lean`, 68000 build, GCC 16.2, unstripped, built from the same source as the
release). **Zune68 = Beta 5 (release v2026.11.22, `Zune68-2026.11.22-amigaos3-m68k.lha`),
`muimaster.library` 21.227 (22.11.2026) "Zune68 Beta 5", as shipped (433,720 bytes, 68000 build).** Its
files were put where its installer puts them: `LIBS:muimaster.library` and the 39 class files of its
`Libs/MUI` in `MUI:Libs/MUI` (the test machines have MUI 3.8's `MUI:` drawer; same-named files are
replaced, as the installer does). The library reports MUI 5 (21.227) on every OS, so `opbench` opens it
as version 19 on OS 3.1 and it hands out MUI 5's behaviour; Zune68 has no switch for a MUI 3.8
identity. A1200 = a1200-31-mui38 (68EC020, OS 3.1, 4 MB fast); A600 = a600-31 (68000, OS 3.1) with
`fast_memory = 4096` and MUI 3.8's `MUI:` drawer and startup assigns copied on; OS 3.2.1 = a1200-32-mui5
(MUI 5 21.227) with `MUI:Libs` moved aside for OpenMUI (`opbench v21`, MUI 5 identity) and for Zune68
(`opbench v21`). MUI 5's "text shorten set" comes from a second pair of runs with `v21`. Zune68 completed
all operations on every machine, the popup included (its popup opened on another screen in the build
measured on 2026-11-12). The ratio columns are the mean of the two instances of the library over the
mean of the two of the reference (MUI 3.8 on OS 3.1, MUI 5 on OS 3.2.1); the last column is Zune68 over
OpenMUI. One A600 OpenMUI instance lost its connection once and was run again alone: its figures are
those of the repeat.

### A1200 (68EC020): OpenMUI and Zune68 Beta 5 against MUI 3.8

| operation | MUI 3.8 inst 1 | MUI 3.8 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | Zune68 inst 1 | Zune68 inst 2 | OpenMUI / MUI 3.8 | Zune68 / MUI 3.8 | Zune68 / OpenMUI |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 92.56 | 88.60 | 16.15 | 15.91 | 14.68 | 14.46 | 0.18 | 0.16 | 0.91 |
| app+windows create | 236.14 | 232.83 | 190.81 | 183.46 | 263.22 | 257.81 | 0.80 | 1.11 | 1.39 |
| window open+close | 520.71 | 517.71 | 401.58 | 400.06 | 600.36 | 601.23 | 0.77 | 1.16 | 1.50 |
| full redraw | 140.55 | 150.54 | 132.76 | 132.96 | 153.40 | 153.29 | 0.91 | 1.05 | 1.15 |
| relayout | 197.53 | 206.88 | 172.50 | 172.67 | 304.37 | 300.12 | 0.85 | 1.49 | 1.75 |
| slider set | 4.10 | 4.04 | 2.72 | 2.72 | 4.44 | 4.44 | 0.67 | 1.09 | 1.63 |
| notify chain | 7.93 | 7.84 | 5.64 | 5.64 | 9.09 | 9.09 | 0.72 | 1.15 | 1.61 |
| cycle set | 2.44 | 2.44 | 2.01 | 2.02 | 4.77 | 4.66 | 0.83 | 1.93 | 2.34 |
| radio set | 15.56 | 15.56 | 8.27 | 8.27 | 21.46 | 21.48 | 0.53 | 1.38 | 2.60 |
| text set | 2.51 | 2.50 | 2.39 | 2.39 | 2.40 | 2.40 | 0.96 | 0.96 | 1.00 |
| list insert 1 | 10.32 | 10.33 | 8.27 | 8.13 | 34.67 | 34.40 | 0.79 | 3.35 | 4.21 |
| list insert 100 | 831.56 | 830.07 | 61.20 | 62.05 | 106.06 | 103.56 | 0.07 | 0.13 | 1.70 |
| list insert 1000 | 6,648.5 | 6,648.8 | 247.80 | 247.95 | 658.85 | 641.86 | 0.04 | 0.10 | 2.62 |
| list active | 21.15 | 21.12 | 19.04 | 18.79 | 23.61 | 23.31 | 0.90 | 1.11 | 1.24 |
| list scroll line | 14.63 | 14.64 | 13.24 | 13.23 | 18.28 | 17.99 | 0.90 | 1.24 | 1.37 |
| list scroll page | 44.42 | 44.38 | 39.29 | 39.06 | 43.10 | 42.78 | 0.88 | 0.97 | 1.10 |
| list sort 1000 | 657.83 | 658.24 | 94.44 | 94.73 | 110.62 | 111.27 | 0.14 | 0.17 | 1.17 |
| button click | 22.25 | 22.52 | 16.59 | 16.58 | 17.94 | 17.91 | 0.74 | 0.80 | 1.08 |
| string type | 7.92 | 7.57 | 6.67 | 6.66 | 10.76 | 10.78 | 0.86 | 1.39 | 1.62 |
| string backspace | 7.83 | 7.48 | 6.60 | 6.58 | 10.46 | 10.42 | 0.86 | 1.36 | 1.58 |
| menu open+close | 13.54 | 13.22 | 13.42 | 13.15 | 19.99 | 19.92 | 0.99 | 1.49 | 1.50 |
| iconify | 84.47 | 83.37 | 78.30 | 77.58 | 84.55 | 83.11 | 0.93 | 1.00 | 1.08 |
| uniconify | 449.77 | 445.97 | 339.56 | 335.70 | 1,018.5 | 996.50 | 0.75 | 2.25 | 2.98 |
| register page | 53.60 | 53.40 | 42.60 | 42.62 | 59.84 | 58.26 | 0.80 | 1.10 | 1.39 |
| popup open+close | 120.01 | 120.00 | 95.87 | 95.63 | 139.96 | 139.69 | 0.80 | 1.17 | 1.46 |
| window resize | 230.07 | 220.15 | 220.10 | 220.13 | 360.11 | 360.13 | 0.98 | 1.60 | 1.64 |

OpenMUI: all 26 at 0.99 or below. Zune68: 7 below 1.00 (prefs+app 0.16, list insert 100 and 1000
0.13 and 0.10, list sort 0.17, button click 0.80, list scroll page 0.97, text set 0.96), iconify level
(1.00), 18 above (1.05 to 3.35). Against OpenMUI, Zune68 is faster only at starting an empty program
(0.91) and level at text set.

### A600 (68000, 4 MB fast): OpenMUI and Zune68 Beta 5 against MUI 3.8

| operation | MUI 3.8 inst 1 | MUI 3.8 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | Zune68 inst 1 | Zune68 inst 2 | OpenMUI / MUI 3.8 | Zune68 / MUI 3.8 | Zune68 / OpenMUI |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 351.12 | 353.47 | 66.02 | 66.34 | 58.60 | 62.72 | 0.19 | 0.17 | 0.92 |
| app+windows create | 931.11 | 932.73 | 744.84 | 749.47 | 1,044.9 | 1,079.3 | 0.80 | 1.14 | 1.42 |
| window open+close | 1,759.2 | 1,757.6 | 1,384.6 | 1,381.3 | 2,170.9 | 2,206.8 | 0.79 | 1.24 | 1.58 |
| full redraw | 474.59 | 474.88 | 441.71 | 441.87 | 530.91 | 531.75 | 0.93 | 1.12 | 1.20 |
| relayout | 701.64 | 702.38 | 607.55 | 607.92 | 1,157.3 | 1,175.5 | 0.87 | 1.66 | 1.92 |
| slider set | 15.12 | 15.11 | 10.06 | 10.09 | 16.05 | 16.06 | 0.67 | 1.06 | 1.59 |
| notify chain | 29.37 | 29.33 | 20.88 | 20.93 | 32.76 | 32.78 | 0.71 | 1.12 | 1.57 |
| cycle set | 9.32 | 9.37 | 7.83 | 7.90 | 19.12 | 19.57 | 0.84 | 2.07 | 2.46 |
| radio set | 60.42 | 60.38 | 32.34 | 32.30 | 82.23 | 82.29 | 0.54 | 1.36 | 2.55 |
| text set | 9.66 | 9.67 | 9.43 | 9.43 | 9.42 | 9.41 | 0.98 | 0.97 | 1.00 |
| list insert 1 | 32.46 | 32.41 | 32.60 | 32.94 | 125.91 | 127.63 | 1.01 | 3.91 | 3.87 |
| list insert 100 | 2,743.8 | 2,744.7 | 226.18 | 226.44 | 417.35 | 427.25 | 0.08 | 0.15 | 1.87 |
| list insert 1000 | 23,924.4 | 23,929.1 | 1,010.7 | 1,008.8 | 2,688.8 | 2,794.3 | 0.04 | 0.11 | 2.71 |
| list active | 64.85 | 65.00 | 60.78 | 61.13 | 77.44 | 79.27 | 0.94 | 1.21 | 1.29 |
| list scroll line | 39.87 | 39.96 | 37.03 | 37.29 | 56.30 | 57.87 | 0.93 | 1.43 | 1.54 |
| list scroll page | 153.63 | 153.56 | 134.00 | 134.58 | 149.89 | 151.40 | 0.87 | 0.98 | 1.12 |
| list sort 1000 | 2,488.8 | 2,493.3 | 382.87 | 380.69 | 434.74 | 433.89 | 0.15 | 0.17 | 1.14 |
| button click | 92.45 | 91.40 | 68.56 | 68.56 | 74.29 | 74.37 | 0.75 | 0.81 | 1.08 |
| string type | 31.47 | 31.62 | 26.28 | 26.27 | 43.78 | 43.80 | 0.83 | 1.39 | 1.67 |
| string backspace | 31.20 | 31.47 | 26.03 | 25.75 | 42.44 | 42.43 | 0.83 | 1.35 | 1.64 |
| menu open+close | 46.48 | 46.46 | 46.43 | 46.13 | 39.85 | 40.13 | 1.00 | 0.86 | 0.86 |
| iconify | 302.50 | 302.76 | 234.87 | 229.92 | 263.48 | 268.58 | 0.77 | 0.88 | 1.14 |
| uniconify | 1,532.0 | 1,539.6 | 1,247.8 | 1,227.8 | 3,938.6 | 4,121.6 | 0.81 | 2.62 | 3.26 |
| register page | 160.74 | 160.38 | 144.94 | 145.05 | 216.80 | 224.16 | 0.90 | 1.37 | 1.52 |
| popup open+close | 400.19 | 407.75 | 338.86 | 339.17 | 484.01 | 489.08 | 0.84 | 1.20 | 1.44 |
| window resize | 618.57 | 620.23 | 520.25 | 520.15 | 789.15 | 811.82 | 0.84 | 1.29 | 1.54 |

OpenMUI: 24 of 26 below 1.00, menu open+close level (1.00), list insert 1 at 1.01. Zune68: 9 below 1.00
(prefs+app 0.17, list insert 100 and 1000 0.15 and 0.11, list sort 0.17, button click 0.81, menu
open+close 0.86, iconify 0.88, list scroll page 0.98, text set 0.97), 17 above (1.06 to 3.91). Against
OpenMUI, Zune68 is faster at prefs+app (0.92) and menu open+close (0.86) and level at text set.

### OS 3.2.1: OpenMUI (MUI 5 identity) and Zune68 Beta 5 against MUI 5 21.227

| operation | MUI 5 inst 1 | MUI 5 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | Zune68 inst 1 | Zune68 inst 2 | OpenMUI / MUI 5 | Zune68 / MUI 5 | Zune68 / OpenMUI |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 300.68 | 298.73 | 18.69 | 18.17 | 16.27 | 15.62 | 0.06 | 0.05 | 0.87 |
| app+windows create | 581.58 | 573.84 | 204.25 | 197.28 | 288.07 | 269.77 | 0.35 | 0.48 | 1.39 |
| window open+close | 1,030.4 | 1,004.6 | 447.70 | 440.82 | 662.57 | 640.69 | 0.44 | 0.64 | 1.47 |
| full redraw | 206.72 | 207.07 | 139.04 | 139.10 | 158.92 | 159.06 | 0.67 | 0.77 | 1.14 |
| relayout | 317.24 | 317.31 | 181.63 | 181.45 | 325.72 | 312.50 | 0.57 | 1.01 | 1.76 |
| slider set | 6.77 | 6.77 | 3.03 | 3.02 | 4.76 | 4.76 | 0.45 | 0.70 | 1.57 |
| notify chain | 12.76 | 12.76 | 6.23 | 6.24 | 9.73 | 9.73 | 0.49 | 0.76 | 1.56 |
| cycle set | 2.74 | 2.75 | 2.05 | 2.05 | 4.06 | 4.05 | 0.75 | 1.48 | 1.98 |
| radio set | 11.91 | 11.92 | 8.39 | 8.40 | 21.79 | 21.80 | 0.70 | 1.83 | 2.60 |
| text set | 2.25 | 2.26 | 1.66 | 1.65 | 1.66 | 1.67 | 0.73 | 0.74 | 1.01 |
| list insert 1 | 22.84 | 22.89 | 9.05 | 9.04 | 34.33 | 34.23 | 0.40 | 1.50 | 3.79 |
| list insert 100 | 2,037.0 | 2,038.9 | 62.33 | 61.74 | 112.14 | 106.04 | 0.03 | 0.05 | 1.76 |
| list insert 1000 | 20,243.9 | 20,281.7 | 248.47 | 248.86 | 733.76 | 674.19 | 0.01 | 0.03 | 2.83 |
| list active | 39.23 | 39.24 | 18.68 | 18.41 | 21.59 | 21.58 | 0.47 | 0.55 | 1.16 |
| list scroll line | 21.12 | 21.14 | 14.58 | 14.58 | 16.85 | 16.84 | 0.69 | 0.80 | 1.16 |
| list scroll page | 57.00 | 57.00 | 39.82 | 39.85 | 41.39 | 41.39 | 0.70 | 0.73 | 1.04 |
| list sort 1000 | 2,305.8 | 2,306.1 | 138.27 | 137.47 | 152.58 | 153.70 | 0.06 | 0.07 | 1.11 |
| button click | 20.77 | 20.78 | 16.70 | 16.66 | 18.16 | 18.56 | 0.80 | 0.88 | 1.10 |
| string type | 14.89 | 14.93 | 7.51 | 7.50 | 10.78 | 10.80 | 0.50 | 0.72 | 1.44 |
| string backspace | 14.24 | 14.30 | 7.36 | 7.41 | 10.46 | 10.48 | 0.52 | 0.73 | 1.42 |
| menu open+close | 27.18 | 25.93 | 19.98 | 19.99 | 19.99 | 19.98 | 0.75 | 0.75 | 1.00 |
| iconify | 195.85 | 194.70 | 100.71 | 96.67 | 104.90 | 102.38 | 0.51 | 0.53 | 1.05 |
| uniconify | 1,213.7 | 1,229.6 | 372.11 | 371.24 | 1,103.8 | 1,063.7 | 0.30 | 0.89 | 2.92 |
| register page | 88.62 | 88.71 | 44.94 | 44.88 | 54.37 | 54.40 | 0.51 | 0.61 | 1.21 |
| popup open+close | 528.09 | 539.88 | 103.83 | 104.11 | 140.10 | 139.94 | 0.19 | 0.26 | 1.35 |
| window resize | 290.11 | 290.10 | 169.93 | 170.05 | 240.08 | 240.11 | 0.59 | 0.83 | 1.41 |
| text shorten set | 4.69 | 4.69 | 3.45 | 3.43 | 3.28 | 3.23 | 0.73 | 0.69 | 0.95 |

OpenMUI: all 27 below 0.81. Zune68: 23 of 27 below 1.00 (0.03 to 0.89), relayout level (1.01), cycle set,
radio set and list insert 1 slower (1.48, 1.83, 1.50). Against OpenMUI, Zune68 is faster at prefs+app
(0.87) and text shorten set (0.95), level at menu open+close and text set.

### Memory (MUI-Demo cold, `measure.sh`, `libonly.sh`, bytes)

OS 3.1 on a1200-31-mui38, OS 3.2.1 on a1200-32-mui5; run twice (library alone once). Zune68 on OS 3.2.1
installed as above with MUI 5's `MUI:Libs` moved aside; OpenMUI there with `ENVARC:OpenMUI/BehaveAs` =
`MUI5`. All six cold boots ran in the same session as the speed figures.

| | first program | each further | library alone | first window |
|---|---:|---:|---:|---:|
| OpenMUI 1.0, OS 3.1 | 427,984 | 193,056-193,120 | 245,920 | 1.32 s |
| MUI 3.8, OS 3.1 | 460,296 | 210,632-210,696 / 218,888 | 238,520 | 1.46-1.56 s |
| Zune68 Beta 5, OS 3.1 | 672,304 | 267,872-267,936 | 433,336 | 1.82 s |
| OpenMUI 1.0, MUI 5 identity, OS 3.2.1 | 434,672 | 201,704-201,744 | 249,800 | 1.68-1.74 s |
| MUI 5 21.227, OS 3.2.1 | 1,293,680 | 427,104-435,480 | 581,024 | 4.34-4.36 s |
| Zune68 Beta 5, OS 3.2.1 | 667,728 | 272,488-272,552 | 437,544 | 1.80-1.84 s |

Files on disk: Zune68 Beta 5 `muimaster.library` 433,720 bytes (all built-in classes inside) and 39
optional class files in `Libs/MUI` (NList, NListtree, TextEditor, TheBar, BetterString and others)
803,100 bytes, loaded only when a program asks for them; OpenMUI 232,036 bytes stripped (253,960
unstripped) and 54 class files in `LIBS:OpenMUI` (289,948 bytes stripped, byte for byte the files of the
1.0 archive; 364,304 unstripped; this section first said 359,108, a figure no strip of these files gives)
for the rarely used built-in classes, its bundled add-on classes (the same kind as
Zune68's 39) on top; MUI 3.8 166,412 and 36 files in `MUI:Libs/mui`
271,208; MUI 5 565,488 and 80 files 2,053,912 (these three as on 2026-11-19).

Raw output: `~/.cache/z68-beta5/out/` (outside the repository); tables in `measurements-2026-11-23.txt`.

## Measurement of 2026-11-23 (2): after the speed round against Zune68

The build after the round of this day (commits 441c72dd, 4911e404: the default settings built once and
shared, Text's background without the dispatch, MUI 5's shortened text without an allocation; library
254,008 bytes unstripped, 232,084 stripped), against the same Zune68 Beta 5 and MUI 3.8 installations as
in the section above and with the same method (`opbench.sh`, the binary of 2026-11-19, medians of 9, two
instances of each library, the instances of a round concurrently, all three libraries in the same
session; OS 3.2.1 with `opbench v21`, MUI 5's text shorten set from a second pair of `v21` runs).
Emulator figures (FS-UAE, cycle-exact, warp mode). Scripts: `final.sh`, `final32.sh` in the round's work
folder (the same commands as `~/.cache/z68-beta5/s/run.sh` and `run32.sh`).

### A1200 (68EC020): OpenMUI and Zune68 Beta 5 against MUI 3.8

| operation | MUI 3.8 inst 1 | MUI 3.8 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | Zune68 inst 1 | Zune68 inst 2 | OpenMUI / MUI 3.8 | Zune68 / MUI 3.8 | Zune68 / OpenMUI |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 90.93 | 91.45 | 9.94 | 9.67 | 14.62 | 14.39 | 0.11 | 0.16 | 1.48 |
| app+windows create | 234.79 | 235.76 | 185.37 | 178.00 | 268.90 | 255.87 | 0.77 | 1.12 | 1.44 |
| window open+close | 520.12 | 520.21 | 401.26 | 398.77 | 601.57 | 581.89 | 0.77 | 1.14 | 1.48 |
| full redraw | 140.65 | 150.68 | 132.79 | 132.77 | 153.63 | 153.04 | 0.91 | 1.05 | 1.15 |
| relayout | 197.64 | 207.23 | 172.57 | 172.59 | 308.40 | 298.55 | 0.85 | 1.50 | 1.76 |
| slider set | 4.09 | 4.04 | 2.72 | 2.72 | 4.45 | 4.44 | 0.67 | 1.09 | 1.63 |
| notify chain | 7.93 | 7.82 | 5.64 | 5.64 | 9.09 | 9.09 | 0.72 | 1.15 | 1.61 |
| cycle set | 2.43 | 2.44 | 2.02 | 2.02 | 4.89 | 4.55 | 0.83 | 1.94 | 2.34 |
| radio set | 15.56 | 15.56 | 8.27 | 8.28 | 21.48 | 21.46 | 0.53 | 1.38 | 2.59 |
| text set | 2.50 | 2.50 | 2.37 | 2.37 | 2.40 | 2.39 | 0.95 | 0.96 | 1.01 |
| list insert 1 | 10.33 | 10.33 | 8.18 | 8.19 | 35.16 | 34.28 | 0.79 | 3.36 | 4.24 |
| list insert 100 | 829.29 | 830.03 | 62.27 | 61.30 | 109.27 | 104.06 | 0.07 | 0.13 | 1.73 |
| list insert 1000 | 6,653.0 | 6,652.3 | 247.72 | 248.72 | 688.38 | 644.60 | 0.04 | 0.10 | 2.69 |
| list active | 21.13 | 21.16 | 18.82 | 18.81 | 24.16 | 23.11 | 0.89 | 1.12 | 1.26 |
| list scroll line | 14.61 | 14.61 | 13.25 | 13.25 | 18.01 | 17.86 | 0.91 | 1.23 | 1.35 |
| list scroll page | 44.41 | 44.39 | 39.17 | 38.97 | 42.74 | 42.56 | 0.88 | 0.96 | 1.09 |
| list sort 1000 | 658.21 | 658.11 | 93.96 | 94.24 | 110.13 | 109.93 | 0.14 | 0.17 | 1.17 |
| button click | 22.53 | 22.58 | 16.57 | 16.53 | 18.01 | 17.92 | 0.73 | 0.80 | 1.09 |
| string type | 7.85 | 7.77 | 6.67 | 6.68 | 10.80 | 10.80 | 0.85 | 1.38 | 1.62 |
| string backspace | 7.76 | 7.68 | 6.61 | 6.57 | 10.44 | 10.43 | 0.85 | 1.35 | 1.58 |
| menu open+close | 13.21 | 13.23 | 13.17 | 13.28 | 19.99 | 19.99 | 1.00 | 1.51 | 1.51 |
| iconify | 84.15 | 84.17 | 76.97 | 77.60 | 86.19 | 82.94 | 0.92 | 1.00 | 1.09 |
| uniconify | 448.92 | 449.50 | 338.82 | 337.71 | 1,036.3 | 982.57 | 0.75 | 2.25 | 2.98 |
| register page | 53.77 | 53.76 | 42.57 | 42.53 | 61.08 | 57.80 | 0.79 | 1.11 | 1.40 |
| popup open+close | 120.01 | 120.00 | 96.01 | 95.35 | 139.96 | 139.95 | 0.80 | 1.17 | 1.46 |
| window resize | 230.11 | 230.11 | 220.10 | 220.09 | 360.15 | 300.29 | 0.96 | 1.44 | 1.50 |

OpenMUI: 25 of 26 below 1.00, menu open+close level (1.00; 13.17-13.28 against 13.21-13.23). Zune68: 7
below 1.00, iconify level. Zune68 over OpenMUI: 1.01 (text set) to 4.24 (list insert 1) in all 26.

### A600 (68000, 4 MB fast): OpenMUI and Zune68 Beta 5 against MUI 3.8

| operation | MUI 3.8 inst 1 | MUI 3.8 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | Zune68 inst 1 | Zune68 inst 2 | OpenMUI / MUI 3.8 | Zune68 / MUI 3.8 | Zune68 / OpenMUI |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 358.84 | 351.61 | 38.08 | 37.26 | 62.70 | 60.82 | 0.11 | 0.17 | 1.64 |
| app+windows create | 935.65 | 935.30 | 735.04 | 727.32 | 1,081.7 | 1,060.7 | 0.78 | 1.15 | 1.46 |
| window open+close | 1,775.1 | 1,761.3 | 1,390.4 | 1,389.9 | 2,189.7 | 2,181.3 | 0.79 | 1.24 | 1.57 |
| full redraw | 474.93 | 474.32 | 442.07 | 441.99 | 532.03 | 531.63 | 0.93 | 1.12 | 1.20 |
| relayout | 702.95 | 702.39 | 608.35 | 607.40 | 1,166.5 | 1,154.1 | 0.87 | 1.65 | 1.91 |
| slider set | 15.12 | 15.09 | 10.09 | 10.11 | 16.07 | 16.06 | 0.67 | 1.06 | 1.59 |
| notify chain | 29.37 | 29.41 | 20.91 | 20.91 | 32.78 | 32.77 | 0.71 | 1.12 | 1.57 |
| cycle set | 9.34 | 9.37 | 7.84 | 7.86 | 19.43 | 19.21 | 0.84 | 2.07 | 2.46 |
| radio set | 60.42 | 60.42 | 32.31 | 32.30 | 82.30 | 82.33 | 0.53 | 1.36 | 2.55 |
| text set | 9.71 | 9.70 | 9.31 | 9.33 | 9.39 | 9.40 | 0.96 | 0.97 | 1.01 |
| list insert 1 | 32.39 | 32.36 | 32.99 | 32.93 | 127.07 | 126.20 | 1.02 | 3.91 | 3.84 |
| list insert 100 | 2,745.5 | 2,743.9 | 227.51 | 226.88 | 422.74 | 418.08 | 0.08 | 0.15 | 1.85 |
| list insert 1000 | 23,937.6 | 23,935.7 | 1,017.3 | 1,012.7 | 2,777.0 | 2,720.4 | 0.04 | 0.11 | 2.71 |
| list active | 64.97 | 64.92 | 61.49 | 61.42 | 79.11 | 77.94 | 0.95 | 1.21 | 1.28 |
| list scroll line | 40.17 | 40.09 | 37.37 | 37.33 | 57.71 | 56.67 | 0.93 | 1.43 | 1.53 |
| list scroll page | 153.55 | 153.39 | 135.23 | 134.78 | 151.11 | 150.26 | 0.88 | 0.98 | 1.12 |
| list sort 1000 | 2,493.2 | 2,490.0 | 378.89 | 385.39 | 434.77 | 435.15 | 0.15 | 0.17 | 1.14 |
| button click | 92.10 | 92.10 | 68.83 | 68.52 | 74.47 | 73.91 | 0.75 | 0.81 | 1.08 |
| string type | 30.32 | 31.42 | 26.28 | 26.31 | 43.86 | 43.87 | 0.85 | 1.42 | 1.67 |
| string backspace | 30.08 | 31.17 | 25.86 | 25.90 | 42.56 | 42.53 | 0.85 | 1.39 | 1.64 |
| menu open+close | 45.44 | 48.07 | 45.51 | 46.51 | 40.13 | 40.13 | 0.98 | 0.86 | 0.87 |
| iconify | 298.61 | 296.58 | 233.28 | 235.87 | 264.89 | 264.81 | 0.79 | 0.89 | 1.13 |
| uniconify | 1,544.3 | 1,535.2 | 1,243.7 | 1,243.4 | 4,002.9 | 3,968.3 | 0.81 | 2.59 | 3.20 |
| register page | 160.70 | 160.59 | 145.03 | 145.12 | 223.06 | 219.09 | 0.90 | 1.38 | 1.52 |
| popup open+close | 404.42 | 411.82 | 340.97 | 339.53 | 496.60 | 480.05 | 0.83 | 1.20 | 1.44 |
| window resize | 620.25 | 610.38 | 530.20 | 520.16 | 809.89 | 790.36 | 0.85 | 1.30 | 1.52 |

OpenMUI: 25 of 26 below 1.00; list insert 1 at 1.02 (32.93-32.99 against 32.36-32.39; release 1.0 32.68-33.33
in this session's runs, List's, not touched). Zune68: 9 below 1.00. Zune68 over OpenMUI: 1.01 (text set)
to 3.84 in 25 of 26; menu open+close 0.87 (below).

### OS 3.2.1: OpenMUI (MUI 5 identity) and Zune68 Beta 5 against MUI 5 21.227

| operation | MUI 5 inst 1 | MUI 5 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | Zune68 inst 1 | Zune68 inst 2 | OpenMUI / MUI 5 | Zune68 / MUI 5 | Zune68 / OpenMUI |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 299.66 | 301.09 | 10.99 | 11.30 | 14.94 | 15.29 | 0.04 | 0.05 | 1.36 |
| app+windows create | 575.27 | 574.23 | 187.38 | 192.45 | 266.29 | 265.16 | 0.33 | 0.46 | 1.40 |
| window open+close | 1,015.5 | 1,005.9 | 438.17 | 441.01 | 635.51 | 640.29 | 0.43 | 0.63 | 1.45 |
| full redraw | 206.37 | 206.34 | 138.95 | 138.84 | 158.94 | 158.91 | 0.67 | 0.77 | 1.14 |
| relayout | 316.67 | 316.81 | 181.48 | 181.60 | 309.94 | 311.57 | 0.57 | 0.98 | 1.71 |
| slider set | 6.75 | 6.76 | 3.03 | 3.02 | 4.75 | 4.76 | 0.45 | 0.70 | 1.57 |
| notify chain | 12.75 | 12.76 | 6.23 | 6.24 | 9.72 | 9.72 | 0.49 | 0.76 | 1.56 |
| cycle set | 2.73 | 2.73 | 2.05 | 2.05 | 4.05 | 4.04 | 0.75 | 1.48 | 1.97 |
| radio set | 11.89 | 11.89 | 8.39 | 8.40 | 21.79 | 21.78 | 0.71 | 1.83 | 2.59 |
| text set | 2.25 | 2.25 | 1.63 | 1.63 | 1.67 | 1.66 | 0.72 | 0.74 | 1.02 |
| list insert 1 | 22.80 | 22.86 | 9.04 | 9.04 | 34.18 | 34.13 | 0.40 | 1.50 | 3.78 |
| list insert 100 | 2,035.6 | 2,034.5 | 62.55 | 62.74 | 104.82 | 104.94 | 0.03 | 0.05 | 1.67 |
| list insert 1000 | 20,235.9 | 20,235.7 | 248.22 | 248.81 | 661.63 | 661.07 | 0.01 | 0.03 | 2.66 |
| list active | 39.18 | 39.18 | 18.69 | 18.55 | 21.56 | 21.55 | 0.48 | 0.55 | 1.16 |
| list scroll line | 21.15 | 21.09 | 14.57 | 14.57 | 16.83 | 16.79 | 0.69 | 0.80 | 1.15 |
| list scroll page | 56.88 | 56.93 | 39.77 | 39.81 | 41.36 | 41.37 | 0.70 | 0.73 | 1.04 |
| list sort 1000 | 2,305.6 | 2,307.9 | 137.04 | 137.33 | 152.37 | 153.59 | 0.06 | 0.07 | 1.12 |
| button click | 21.16 | 21.21 | 16.63 | 16.62 | 18.45 | 18.08 | 0.78 | 0.86 | 1.10 |
| string type | 15.26 | 15.34 | 7.48 | 7.48 | 10.78 | 10.79 | 0.49 | 0.70 | 1.44 |
| string backspace | 14.61 | 14.73 | 7.43 | 7.42 | 10.48 | 10.45 | 0.51 | 0.71 | 1.41 |
| menu open+close | 25.90 | 26.19 | 19.99 | 19.98 | 19.99 | 19.98 | 0.77 | 0.77 | 1.00 |
| iconify | 197.27 | 192.33 | 96.55 | 96.74 | 101.62 | 101.10 | 0.50 | 0.52 | 1.05 |
| uniconify | 1,219.8 | 1,218.6 | 372.12 | 372.36 | 1,063.5 | 1,044.1 | 0.31 | 0.86 | 2.83 |
| register page | 88.57 | 88.59 | 44.84 | 44.93 | 54.31 | 54.30 | 0.51 | 0.61 | 1.21 |
| popup open+close | 535.68 | 532.09 | 103.35 | 104.00 | 139.61 | 140.10 | 0.19 | 0.26 | 1.35 |
| window resize | 290.10 | 290.11 | 170.19 | 170.28 | 240.11 | 240.16 | 0.59 | 0.83 | 1.41 |
| text shorten set | 4.68 | 4.69 | 3.12 | 3.12 | 3.22 | 3.21 | 0.67 | 0.69 | 1.03 |

OpenMUI: all 27 below 0.79. Zune68: 24 of 27 below 1.00 (relayout 0.98), cycle set, radio set and list
insert 1 at 1.48, 1.83 and 1.50. Zune68 over OpenMUI: 1.02 (text set) to 3.78 in 26 of 27; menu
open+close level (both 20.0 ms).

### Menus: what "menu open+close" measures

`menu open+close` writes the right button down and up into input.device and runs the program's loop
until it has nothing more to do. A window that asks for IDCMP_MENUVERIFY (Zune68 always; OpenMUI only
for programs that get MUI 4/5) makes Intuition wait for the program's answer, which Intuition takes at
its next input tick; with the button already up by then no menu is shown, and the measurement ends
before that tick. The figures: on the A1200 one tick (20.0 ms), on the A600 two (40.1 ms), whatever the
menu costs; an OpenMUI build asking for MENUVERIFY in MUI 3 programs too measured exactly that (19.9 and
39.8 ms; not kept, MUI 3.8 does not ask). Two operations added at the end of `tests/native/opbench.c`
(the 25 before run as before) wait for the menus: `menu shown+close` runs the loop until the window is in
its menu state (WFLG_MENUSTATE) and out of it again, `menu select` also moves onto the first title and
its first item and waits for MUIA_Application_MenuAction. One instance each (OpenMUI and MUI 3.8 on the
A1200 also a second, and release 1.0):

| | A1200 OpenMUI | A1200 1.0 | A1200 MUI 3.8 | A1200 Zune68 | A600 OpenMUI | A600 1.0 | A600 MUI 3.8 | A600 Zune68 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| menu open+close | 13.2-13.4 | | 13.8 | 20.0 | 46.3 | | 46.6 | 39.8 |
| menu shown+close | 13.1-13.2 | 13.1 | 13.2-13.3 | 120.0 | 46.5-46.6 | 49.0 | 45.7 | 120.0 |
| menu select | 47.0-47.5 | 46.5-47.5 | 45.3-46.5 | 120.0 | 158.9-159.3 | 160.7 | 156.5 | 248.3 |

Zune68's menus take six ticks (120 ms) from the button press to the closed menu on both machines.
`menu select` is 1.01-1.05 of MUI 3.8's on the A1200 and 1.02 on the A600, release 1.0 the same (not traced; the OS
call counts of the program's own task are no higher than MUI 3.8's: 3 SetAttrsA, 4 GetAttr and 17 GetMsg
against 7, the rest Intuition's).

### Memory (MUI-Demo cold, `measure.sh`, `libonly.sh`, bytes)

The same machines and method as above; release 1.0 again in the same session. MUI 3.8, MUI 5 and Zune68
are the figures of the section above (the same files).

| | first program | each further | library alone | first window |
|---|---:|---:|---:|---:|
| OpenMUI after the round, OS 3.1 | 427,984 | 191,856-191,920 | 245,864 | 1.30 s |
| OpenMUI 1.0, OS 3.1 | 427,984 | 193,056-193,120 | 245,920 | 1.30 s |
| OpenMUI after the round, MUI 5 identity, OS 3.2.1 | 434,672 | 200,504-200,544 | 249,792 | 1.66-1.70 s |
| OpenMUI 1.0, MUI 5 identity, OS 3.2.1 | 434,672 | 201,704-201,744 | 249,800 | 1.68-1.70 s |

Each further program 1,200 bytes less: the settings (1,208 bytes) are shared while no settings file is
read; the first program allocates them instead of keeping them in its instance data (the same bytes).
Files: library 232,084 bytes stripped (1.0: 232,036), class files 290,056 (1.0: 289,948; `Zmui45.mui`
+108).

Raw output: `~/.cache/z68-beta5/out-2026-11-23b/` (outside the repository).

## Measurement of 2026-11-23 (3): after the round `opt1` (settings shared, smaller objects, List, mouse moves)

The build of branch `opt1` (commits f4805b11 to eed8b621; library 256,840 bytes unstripped, 234,760
stripped), against MUI 3.8, Zune68 Beta 5 (2026.11.22) and the build before the round ("1.0" in the
tables: release 1.0 with the speed round of the same day, section (2) above), all in one session with
the method of the sections above (`opbench`, the binary of 2026-11-19, medians of 9, two instances of
each library, cycle-exact FS-UAE in warp mode, five instances at a time). OS 3.2.1 with `opbench v21`;
MUI 5 itself as version 19 and, for text shorten set, a second pair as version 21. Emulator figures.
The runner: `fin.sh`, `mdtab.py` in the round's work folder (the commands of
`tests/lean/scripts/opbench.sh`).

### A1200 (68EC020): OpenMUI, the build before and Zune68 Beta 5 against MUI 3.8

| operation | MUI 3.8 inst 1 | MUI 3.8 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | 1.0 inst 1 | 1.0 inst 2 | Zune68 inst 1 | Zune68 inst 2 | OpenMUI / MUI 3.8 | 1.0 / MUI 3.8 | Zune68 / MUI 3.8 | Zune68 / OpenMUI | OpenMUI / 1.0 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 91.89 | 91.89 | 9.63 | 9.82 | 9.66 | 9.41 | 14.44 | 14.68 | 0.11 | 0.10 | 0.16 | 1.50 | 1.02 |
| app+windows create | 236.80 | 236.44 | 179.64 | 176.62 | 179.27 | 177.09 | 264.24 | 265.19 | 0.75 | 0.75 | 1.12 | 1.49 | 1.00 |
| window open+close | 521.89 | 521.23 | 401.20 | 399.85 | 399.92 | 397.11 | 601.36 | 603.85 | 0.77 | 0.76 | 1.16 | 1.50 | 1.01 |
| full redraw | 150.74 | 150.64 | 133.08 | 133.02 | 132.89 | 133.00 | 153.62 | 153.64 | 0.88 | 0.88 | 1.02 | 1.15 | 1.00 |
| relayout | 207.03 | 207.17 | 172.84 | 172.88 | 172.60 | 172.71 | 309.83 | 308.85 | 0.83 | 0.83 | 1.49 | 1.79 | 1.00 |
| slider set | 4.04 | 4.04 | 2.72 | 2.72 | 2.72 | 2.72 | 4.44 | 4.44 | 0.67 | 0.67 | 1.10 | 1.63 | 1.00 |
| notify chain | 7.82 | 7.81 | 5.64 | 5.64 | 5.64 | 5.64 | 9.09 | 9.10 | 0.72 | 0.72 | 1.16 | 1.61 | 1.00 |
| cycle set | 2.44 | 2.44 | 2.02 | 2.02 | 2.01 | 2.01 | 4.68 | 4.87 | 0.83 | 0.83 | 1.96 | 2.37 | 1.00 |
| radio set | 15.56 | 15.56 | 8.28 | 8.28 | 8.27 | 8.27 | 21.49 | 21.49 | 0.53 | 0.53 | 1.38 | 2.60 | 1.00 |
| text set | 2.50 | 2.50 | 2.36 | 2.37 | 2.37 | 2.37 | 2.40 | 2.40 | 0.95 | 0.95 | 0.96 | 1.01 | 1.00 |
| list insert 1 | 10.33 | 10.35 | 7.35 | 7.30 | 8.19 | 8.12 | 34.83 | 34.43 | 0.71 | 0.79 | 3.35 | 4.73 | 0.90 |
| list insert 100 | 830.54 | 830.25 | 61.57 | 61.10 | 61.37 | 60.60 | 108.67 | 108.55 | 0.07 | 0.07 | 0.13 | 1.77 | 1.01 |
| list insert 1000 | 6,654.2 | 6,650.9 | 248.70 | 247.87 | 247.89 | 247.88 | 674.17 | 683.35 | 0.04 | 0.04 | 0.10 | 2.73 | 1.00 |
| list active | 21.10 | 21.15 | 19.05 | 18.81 | 18.81 | 18.46 | 23.65 | 23.24 | 0.90 | 0.88 | 1.11 | 1.24 | 1.02 |
| list scroll line | 14.61 | 14.59 | 13.36 | 13.30 | 13.30 | 13.24 | 18.27 | 18.40 | 0.91 | 0.91 | 1.26 | 1.38 | 1.00 |
| list scroll page | 44.45 | 44.44 | 39.56 | 39.39 | 39.21 | 38.99 | 43.02 | 43.09 | 0.89 | 0.88 | 0.97 | 1.09 | 1.01 |
| list sort 1000 | 658.76 | 658.50 | 95.38 | 95.99 | 93.39 | 94.32 | 111.50 | 110.43 | 0.15 | 0.14 | 0.17 | 1.16 | 1.02 |
| button click | 22.94 | 22.54 | 16.08 | 16.06 | 16.54 | 16.55 | 17.95 | 17.96 | 0.71 | 0.73 | 0.79 | 1.12 | 0.97 |
| string type | 7.51 | 7.97 | 6.61 | 6.61 | 6.69 | 6.67 | 10.76 | 10.79 | 0.85 | 0.86 | 1.39 | 1.63 | 0.99 |
| string backspace | 7.41 | 7.86 | 6.55 | 6.54 | 6.56 | 6.62 | 10.47 | 10.43 | 0.86 | 0.86 | 1.37 | 1.60 | 0.99 |
| menu open+close | 13.16 | 13.21 | 13.02 | 13.10 | 13.16 | 13.13 | 19.92 | 19.93 | 0.99 | 1.00 | 1.51 | 1.53 | 0.99 |
| iconify | 83.87 | 84.28 | 76.62 | 75.85 | 77.52 | 77.37 | 84.79 | 85.78 | 0.91 | 0.92 | 1.01 | 1.12 | 0.98 |
| uniconify | 446.23 | 448.67 | 339.00 | 337.74 | 337.36 | 337.17 | 1,036.1 | 1,036.4 | 0.76 | 0.75 | 2.32 | 3.06 | 1.00 |
| register page | 53.71 | 53.77 | 42.72 | 42.69 | 42.60 | 42.61 | 59.75 | 60.89 | 0.79 | 0.79 | 1.12 | 1.41 | 1.00 |
| popup open+close | 120.02 | 120.02 | 96.13 | 95.13 | 94.97 | 95.35 | 139.95 | 139.76 | 0.80 | 0.79 | 1.17 | 1.46 | 1.00 |
| window resize | 280.12 | 230.06 | 220.08 | 220.09 | 220.10 | 220.07 | 360.08 | 360.16 | 0.86 | 0.86 | 1.41 | 1.64 | 1.00 |

OpenMUI: all 26 at or below MUI 3.8's time (menu open+close 0.99: 13.02-13.10 against 13.16-13.21).
Against the build before: list insert 1 0.90, button click 0.97, string type and backspace 0.99; the
rest within what two instances of one build differ by on this CPU. In this run list sort 1000 is
95.4-96.0 against 93.4-94.3, list active 18.8-19.1 against 18.5-18.8 and list scroll page 39.4-39.6
against 39.0-39.2; five more instances of those operations alone gave list sort 96.1-97.2 for this
build, 95.0-96.3 for the same code with 156 bytes of padding before List's, and 95.1 for the build
before, with list active 18.5-18.7 against the build before's 19.0: where the library's code lies in
memory and in the 68020's instruction cache, not the code. The A600 has no cache and shows none of it.
Zune68 over OpenMUI: 1.01 (text set) to 4.73 (list insert 1) in all 26; the narrow ones: list scroll
page 1.09, button click 1.12 (was 1.09), iconify 1.12 (1.09).

### A600 (68000, 4 MB fast): the same

| operation | MUI 3.8 inst 1 | MUI 3.8 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | 1.0 inst 1 | 1.0 inst 2 | Zune68 inst 1 | Zune68 inst 2 | OpenMUI / MUI 3.8 | 1.0 / MUI 3.8 | Zune68 / MUI 3.8 | Zune68 / OpenMUI | OpenMUI / 1.0 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 361.26 | 355.72 | 36.61 | 37.95 | 38.25 | 37.27 | 62.64 | 61.64 | 0.10 | 0.11 | 0.17 | 1.67 | 0.99 |
| app+windows create | 935.45 | 936.82 | 708.65 | 728.93 | 751.45 | 735.83 | 1,080.7 | 1,055.4 | 0.77 | 0.79 | 1.14 | 1.49 | 0.97 |
| window open+close | 1,764.7 | 1,761.0 | 1,389.1 | 1,391.8 | 1,398.7 | 1,390.8 | 2,196.3 | 2,177.0 | 0.79 | 0.79 | 1.24 | 1.57 | 1.00 |
| full redraw | 471.29 | 470.89 | 442.24 | 442.09 | 442.06 | 442.09 | 533.24 | 532.29 | 0.94 | 0.94 | 1.13 | 1.20 | 1.00 |
| relayout | 698.98 | 699.46 | 608.94 | 608.48 | 609.01 | 608.16 | 1,169.1 | 1,155.2 | 0.87 | 0.87 | 1.66 | 1.91 | 1.00 |
| slider set | 15.71 | 15.74 | 10.13 | 10.14 | 10.14 | 10.16 | 16.06 | 16.05 | 0.64 | 0.65 | 1.02 | 1.58 | 1.00 |
| notify chain | 30.65 | 30.62 | 20.98 | 20.93 | 20.94 | 20.95 | 32.88 | 32.80 | 0.68 | 0.68 | 1.07 | 1.57 | 1.00 |
| cycle set | 9.36 | 9.34 | 7.89 | 7.87 | 7.87 | 7.87 | 19.32 | 19.02 | 0.84 | 0.84 | 2.05 | 2.43 | 1.00 |
| radio set | 60.56 | 60.44 | 32.39 | 32.32 | 32.40 | 32.44 | 82.42 | 82.25 | 0.53 | 0.54 | 1.36 | 2.54 | 1.00 |
| text set | 9.71 | 9.70 | 9.33 | 9.32 | 9.34 | 9.33 | 9.41 | 9.42 | 0.96 | 0.96 | 0.97 | 1.01 | 1.00 |
| list insert 1 | 32.49 | 32.42 | 28.76 | 29.15 | 33.33 | 32.65 | 127.18 | 125.86 | 0.89 | 1.02 | 3.90 | 4.37 | 0.88 |
| list insert 100 | 2,744.8 | 2,745.7 | 223.65 | 226.14 | 229.20 | 226.07 | 422.20 | 413.24 | 0.08 | 0.08 | 0.15 | 1.86 | 0.99 |
| list insert 1000 | 23,955.6 | 23,960.7 | 1,012.7 | 1,008.3 | 1,018.4 | 1,011.0 | 2,786.8 | 2,702.4 | 0.04 | 0.04 | 0.11 | 2.72 | 1.00 |
| list active | 65.02 | 64.96 | 60.88 | 61.59 | 62.05 | 60.81 | 79.32 | 77.82 | 0.94 | 0.95 | 1.21 | 1.28 | 1.00 |
| list scroll line | 39.90 | 40.03 | 37.23 | 37.50 | 37.52 | 37.21 | 57.80 | 56.48 | 0.93 | 0.93 | 1.43 | 1.53 | 1.00 |
| list scroll page | 153.82 | 154.26 | 134.49 | 135.24 | 135.89 | 134.43 | 151.26 | 150.46 | 0.88 | 0.88 | 0.98 | 1.12 | 1.00 |
| list sort 1000 | 2,497.5 | 2,505.3 | 386.45 | 384.77 | 383.47 | 387.76 | 434.77 | 440.40 | 0.15 | 0.15 | 0.17 | 1.13 | 1.00 |
| button click | 92.41 | 92.37 | 66.97 | 66.97 | 68.86 | 68.48 | 73.99 | 74.46 | 0.72 | 0.74 | 0.80 | 1.11 | 0.98 |
| string type | 31.91 | 31.75 | 26.15 | 26.21 | 26.33 | 26.36 | 43.94 | 44.23 | 0.82 | 0.83 | 1.38 | 1.68 | 0.99 |
| string backspace | 31.66 | 31.58 | 25.66 | 25.66 | 26.12 | 26.12 | 42.42 | 42.71 | 0.81 | 0.83 | 1.35 | 1.66 | 0.98 |
| menu open+close | 46.64 | 46.65 | 47.46 | 45.36 | 45.61 | 46.15 | 41.07 | 39.84 | 0.99 | 0.98 | 0.87 | 0.87 | 1.01 |
| iconify | 303.56 | 301.44 | 229.67 | 232.13 | 271.60 | 235.87 | 265.35 | 262.93 | 0.76 | 0.84 | 0.87 | 1.14 | 0.91 |
| uniconify | 1,542.7 | 1,543.4 | 1,216.9 | 1,247.9 | 1,231.9 | 1,243.4 | 4,058.0 | 3,944.3 | 0.80 | 0.80 | 2.59 | 3.25 | 1.00 |
| register page | 160.76 | 163.83 | 145.67 | 145.54 | 145.59 | 145.55 | 223.74 | 215.96 | 0.90 | 0.90 | 1.35 | 1.51 | 1.00 |
| popup open+close | 407.66 | 404.27 | 341.34 | 341.99 | 345.30 | 340.77 | 499.22 | 480.62 | 0.84 | 0.84 | 1.21 | 1.43 | 1.00 |
| window resize | 621.13 | 620.22 | 520.18 | 470.56 | 580.32 | 520.35 | 810.42 | 789.18 | 0.80 | 0.89 | 1.29 | 1.61 | 0.90 |

OpenMUI: all 26 at or below MUI 3.8's time (menu open+close 0.99); **list insert 1 0.89** (28.8-29.2
against 32.4-32.5; the build before 1.02). Against the build before: list insert 1 0.88, button click
0.97, string backspace 0.98, the rest within the spread (window resize and iconify land on whole ticks:
471-520 against 520-580, 230-232 against 236-272). Zune68 over OpenMUI: 1.01 to 4.37 in 25 of 26; menu
open+close 0.87 (it stops before Zune68's menu is shown, section (2)).

### OS 3.2.1: OpenMUI (MUI 5 identity) and Zune68 Beta 5 against MUI 5 21.227

| operation | MUI 5 inst 1 | MUI 5 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | 1.0 inst 1 | 1.0 inst 2 | Zune68 inst 1 | Zune68 inst 2 | OpenMUI / MUI 5 | 1.0 / MUI 5 | Zune68 / MUI 5 | Zune68 / OpenMUI | OpenMUI / 1.0 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 304.31 | 303.17 | 11.62 | 11.71 | 11.76 | 10.89 | 15.61 | 15.36 | 0.04 | 0.04 | 0.05 | 1.33 | 1.03 |
| app+windows create | 578.56 | 577.71 | 190.88 | 202.36 | 196.43 | 185.60 | 270.11 | 267.67 | 0.34 | 0.33 | 0.47 | 1.37 | 1.03 |
| window open+close | 1,023.2 | 1,029.0 | 440.25 | 444.12 | 443.77 | 437.25 | 640.43 | 639.31 | 0.43 | 0.43 | 0.62 | 1.45 | 1.00 |
| full redraw | 206.25 | 206.82 | 139.20 | 139.01 | 139.22 | 139.04 | 159.05 | 159.07 | 0.67 | 0.67 | 0.77 | 1.14 | 1.00 |
| relayout | 317.17 | 317.31 | 181.68 | 181.52 | 181.48 | 181.54 | 313.44 | 311.79 | 0.57 | 0.57 | 0.99 | 1.72 | 1.00 |
| slider set | 6.76 | 6.77 | 3.02 | 3.02 | 3.02 | 3.02 | 4.76 | 4.76 | 0.45 | 0.45 | 0.70 | 1.58 | 1.00 |
| notify chain | 12.76 | 12.78 | 6.22 | 6.23 | 6.23 | 6.23 | 9.73 | 9.74 | 0.49 | 0.49 | 0.76 | 1.56 | 1.00 |
| cycle set | 2.73 | 2.74 | 2.05 | 2.05 | 2.06 | 2.05 | 4.05 | 4.05 | 0.75 | 0.75 | 1.48 | 1.97 | 1.00 |
| radio set | 11.90 | 11.91 | 8.41 | 8.40 | 8.40 | 8.40 | 21.79 | 21.79 | 0.71 | 0.71 | 1.83 | 2.59 | 1.00 |
| text set | 2.25 | 2.25 | 1.63 | 1.63 | 1.63 | 1.63 | 1.67 | 1.67 | 0.72 | 0.72 | 0.74 | 1.02 | 1.00 |
| list insert 1 | 22.84 | 22.90 | 7.31 | 7.30 | 9.05 | 9.04 | 34.22 | 34.19 | 0.32 | 0.40 | 1.50 | 4.68 | 0.81 |
| list insert 100 | 2,036.4 | 2,039.7 | 61.21 | 59.24 | 62.42 | 61.74 | 106.86 | 106.22 | 0.03 | 0.03 | 0.05 | 1.77 | 0.97 |
| list insert 1000 | 20,244.1 | 20,302.0 | 247.36 | 248.25 | 248.72 | 249.06 | 678.30 | 670.21 | 0.01 | 0.01 | 0.03 | 2.72 | 1.00 |
| list active | 39.20 | 39.25 | 18.67 | 18.72 | 18.59 | 18.60 | 21.59 | 21.56 | 0.48 | 0.47 | 0.55 | 1.15 | 1.01 |
| list scroll line | 21.10 | 21.19 | 14.61 | 14.57 | 14.56 | 14.56 | 16.85 | 16.84 | 0.69 | 0.69 | 0.80 | 1.15 | 1.00 |
| list scroll page | 56.90 | 57.04 | 39.84 | 39.94 | 39.84 | 39.81 | 41.34 | 41.34 | 0.70 | 0.70 | 0.73 | 1.04 | 1.00 |
| list sort 1000 | 2,308.4 | 2,308.5 | 137.95 | 137.72 | 138.19 | 136.97 | 153.49 | 154.35 | 0.06 | 0.06 | 0.07 | 1.12 | 1.00 |
| button click | 21.33 | 21.12 | 16.18 | 16.23 | 17.08 | 16.67 | 18.17 | 18.21 | 0.76 | 0.80 | 0.86 | 1.12 | 0.96 |
| string type | 15.37 | 15.26 | 7.49 | 7.48 | 7.50 | 7.53 | 10.80 | 10.80 | 0.49 | 0.49 | 0.71 | 1.44 | 1.00 |
| string backspace | 14.75 | 14.63 | 7.38 | 7.38 | 7.40 | 7.43 | 10.49 | 10.44 | 0.50 | 0.50 | 0.71 | 1.42 | 1.00 |
| menu open+close | 26.07 | 26.02 | 19.92 | 19.99 | 19.99 | 19.99 | 19.99 | 19.98 | 0.77 | 0.77 | 0.77 | 1.00 | 1.00 |
| iconify | 205.03 | 199.17 | 96.76 | 98.63 | 101.76 | 96.13 | 102.34 | 102.92 | 0.48 | 0.49 | 0.51 | 1.05 | 0.99 |
| uniconify | 1,234.9 | 1,223.0 | 371.23 | 372.79 | 372.10 | 371.64 | 1,063.3 | 1,064.1 | 0.30 | 0.30 | 0.87 | 2.86 | 1.00 |
| register page | 88.64 | 88.73 | 44.94 | 45.01 | 44.93 | 44.91 | 54.37 | 54.36 | 0.51 | 0.51 | 0.61 | 1.21 | 1.00 |
| popup open+close | 557.34 | 548.94 | 103.52 | 104.52 | 103.39 | 102.93 | 139.85 | 139.80 | 0.19 | 0.19 | 0.25 | 1.34 | 1.01 |
| window resize | 290.09 | 290.11 | 170.05 | 170.04 | 170.07 | 170.06 | 240.13 | 240.08 | 0.59 | 0.59 | 0.83 | 1.41 | 1.00 |
| text shorten set | 4.69 | 4.69 | 3.11 | 3.11 | 3.11 | 3.12 | 3.21 | 3.21 | 0.66 | 0.66 | 0.68 | 1.03 | 1.00 |

OpenMUI: all 27 at 0.77 of MUI 5's time or below; against the build before list insert 1 0.81, button
click 0.96, the rest within the spread. Zune68: 24 of 27 below MUI 5's time (relayout 0.99); cycle
set, radio set and list insert 1 at 1.48, 1.83 and 1.50. Zune68 over OpenMUI: 1.02 (text set) to 4.68
(list insert 1) in 26 of 27; menu open+close level (both 20.0 ms: both ask for IDCMP_MENUVERIFY with
MUI 5's identity).

### Menus (the opbench of this repository: `menu shown+close`, `menu select`)

A1200:

| operation | MUI 3.8 inst 1 | MUI 3.8 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | 1.0 inst 1 | 1.0 inst 2 | Zune68 inst 1 | Zune68 inst 2 | OpenMUI / MUI 3.8 | 1.0 / MUI 3.8 | Zune68 / MUI 3.8 | Zune68 / OpenMUI | OpenMUI / 1.0 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| menu open+close | 13.22 | 13.15 | 13.10 | 13.10 | 13.17 | 13.16 | 19.99 | 19.99 | 0.99 | 1.00 | 1.52 | 1.53 | 0.99 |
| menu shown+close | 13.26 | 13.26 | 13.08 | 13.12 | 13.17 | 13.17 | 120.06 | 119.93 | 0.99 | 0.99 | 9.05 | 9.16 | 0.99 |
| menu select | 46.57 | 45.43 | 45.96 | 46.21 | 47.13 | 47.89 | 120.67 | 120.00 | 1.00 | 1.03 | 2.62 | 2.61 | 0.97 |

A600:

| operation | MUI 3.8 inst 1 | MUI 3.8 inst 2 | OpenMUI inst 1 | OpenMUI inst 2 | 1.0 inst 1 | 1.0 inst 2 | Zune68 inst 1 | Zune68 inst 2 | OpenMUI / MUI 3.8 | 1.0 / MUI 3.8 | Zune68 / MUI 3.8 | Zune68 / OpenMUI | OpenMUI / 1.0 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| menu open+close | 46.83 | 46.37 | 45.89 | 46.30 | 46.42 | 45.54 | 39.85 | 40.13 | 0.99 | 0.99 | 0.86 | 0.87 | 1.00 |
| menu shown+close | 48.72 | 46.52 | 46.35 | 46.25 | 49.02 | 46.31 | 118.72 | 120.00 | 0.97 | 1.00 | 2.51 | 2.58 | 0.97 |
| menu select | 155.70 | 155.64 | 154.81 | 156.76 | 158.44 | 159.13 | 242.84 | 253.85 | 1.00 | 1.02 | 1.60 | 1.59 | 0.98 |

**Menu select** is at MUI 3.8's time on both machines now (A1200 46.0-46.2 against 45.4-46.6, A600
154.8-156.8 against 155.6-155.7; the build before 1.03 and 1.02): the window no longer has the mouse's
moves reported while nothing asks for them, and the input method no longer calls GetMsg on empty
ports (3.6 GetMsg per menu select against 15.6; MUI 3.8 7.8, `opbench count`). Menu shown+close lands
on whole input ticks: on the A600 46.3 against MUI 3.8's 46.5-48.7 here, 48.8-49.3 against 45.6-48.1
in an earlier run of the same session.

### An application's start with a settings file

`opbench prof "prefs+app (empty)"` with `tools/lean/prefsmutate.py 0`'s file (160 settings, 2,428
bytes) as `ENV:OpenMUI/global.prefs`, one instance each (the build of commit f4805b11; that path is
the same in the final build):

| | the build before | OpenMUI |
|---|---:|---:|
| A1200, a full global.prefs | 59.3 | 25.2 |
| A1200, no settings file | 10.0 | 9.9 |
| A600, a full global.prefs | 238.9 | 92.1 |
| A600, no settings file | 37.5 | 37.2 |

The file is read and parsed by the first application; each further one asks for the file's date, size
and key (Lock, Examine, UnLock) and looks for its own file (two Opens that fail); 40 % of what is left
is the task waiting for those five packets (pcsample).

### Memory (MUI-Demo cold, `measure.sh`, `libonly.sh`, bytes)

| | first program | each further | library alone | first window |
|---|---:|---:|---:|---:|
| OpenMUI, OS 3.1 | 427,288 | 188,544-188,608 | 248,472 | 1.28-1.30 s |
| the build before, OS 3.1 | 427,984 | 191,856-191,920 | 245,912 | 1.39-1.42 s |
| MUI 3.8 | 460,296 | 210,632-218,888 | 238,520 | 1.6-2.2 s |
| Zune68 Beta 5, OS 3.1 | 672,304 | 267,936 | 433,336 | 1.8 s |
| OpenMUI, MUI 5 identity, OS 3.2.1 | 433,928-433,952 | 197,144-197,184 | 252,352 | 1.62 s |
| the build before, MUI 5 identity, OS 3.2.1 | 434,672 | 200,440-200,544 | 249,792 | 1.72-1.74 s |
| MUI 5 21.227, OS 3.2.1 | 1,293,680 | 427,168-435,480 | 581,024 | 4.3-4.5 s |
| Zune68 Beta 5, OS 3.2.1 | 667,728 | 272,448-272,552 | 437,544 | 1.86-1.94 s |

Each further program 3,312 bytes less (the objects: `README.md`, round `opt1`), the first 696 less
although the library's code is 2,560 bytes more (the shared settings, List's and Window's changes).
The first-window times are host times of the measuring script, not cycle counts. Files: library
234,760 bytes stripped (the build before 232,084), class files 290,120 (290,056).

MUI-Demo's live blocks (`memtally`, the second copy, OpenMUI before the objects were made smaller):
382 objects, 72,008 bytes of the program's 119,050: 72 Texts, 72 Images, 58 Groups, 45 Rectangles, 20
Props, 13 Scrollbars, 9 Windows; 262 notification blocks, 7,936 bytes. Instance sizes against MUI
3.8's for every class: `tests/native/instsizes.c` (`MUI_GetClass(name)->cl_InstSize`).

Raw output: the round's work folder (outside the repository; deleted after the round).

## Measurement of 2026-11-25: after the round `trim1` (Zune's leftovers cut)

The build of branch `trim1` (164b3637; library 254,156 bytes unstripped, 232,100 stripped) against the
build it started from (`lean` 47730569, "lean"), MUI 3.8 (MUI 5 21.227 on OS 3.2.1) and Zune68 Beta 5
(2026.11.22), all in one session: `opbench` (the binary of 2026-11-19), medians of 9, cycle-exact
FS-UAE in warp mode, two instances of MUI 3.8/MUI 5 and Zune68, four of lean and trim1 (two rounds),
eight instances at a time. Ranges over the instances, ratios of the means. Emulator figures.

New this time, asked for by our partner (the most common A1200 today): **A1200/030**, a1200-31-mui38
booted with `amiga-test ... --cpu 030 --config uae_z3mem_size=16` plus the cycle-exact settings: a
68030 at 50 MHz (`uae_cpu_frequency = 50000000`) with a 68882, instruction and data caches on (`cpu`:
"INST: Cache Burst, DATA: Cache NoBurst"), 16 MB Zorro III + the machine's 4 MB fast RAM. FS-UAE
emulates the 030's caches only in cycle-exact mode; the timing is FS-UAE's 030, not a real card's.

### A1200 (68EC020, OS 3.1)

| operation | MUI 3.8 | lean | trim1 | Zune68 Beta 5 | trim1 / lean | trim1 / MUI 3.8 | Zune68 / trim1 |
|---|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 92.18-92.63 | 9.24-9.97 | 9.05-9.28 | 14.81-14.83 | 0.96 | 0.10 | 1.62 |
| app+windows create | 233.8-239.3 | 173.7-180.3 | 173.2-179.0 | 265.3-266.2 | 1.00 | 0.75 | 1.50 |
| window open+close | 520.2-526.2 | 397.7-399.9 | 398.9-400.5 | 600.6-601.5 | 1.00 | 0.76 | 1.50 |
| full redraw | 150.7 | 132.8-132.9 | 132.7-132.8 | 153.4 | 1.00 | 0.88 | 1.16 |
| relayout | 207.0-207.1 | 172.5-172.8 | 172.4-172.6 | 306.1-308.0 | 1.00 | 0.83 | 1.78 |
| slider set | 4.03-4.04 | 2.72 | 2.71-2.72 | 4.44 | 1.00 | 0.67 | 1.64 |
| notify chain | 7.81-7.83 | 5.63-5.64 | 5.63-5.64 | 9.09 | 1.00 | 0.72 | 1.61 |
| cycle set | 2.44-2.45 | 2.02 | 2.01-2.02 | 4.86-4.87 | 1.00 | 0.83 | 2.42 |
| radio set | 15.55-15.56 | 8.25-8.28 | 8.26-8.28 | 21.47 | 1.00 | 0.53 | 2.60 |
| text set | 2.50 | 2.36-2.37 | 2.37 | 2.40 | 1.00 | 0.95 | 1.01 |
| list insert 1 | 10.33 | 7.18-7.24 | 7.17-7.33 | 34.97-35.06 | 1.00 | 0.70 | 4.86 |
| list insert 100 | 829.4-830.3 | 60.22-61.25 | 60.52-60.96 | 107.5-108.4 | 1.00 | 0.07 | 1.78 |
| list insert 1000 | 6,649.3-6,650.8 | 246.6-248.0 | 247.1-248.7 | 673.5-676.9 | 1.00 | 0.04 | 2.72 |
| list active | 21.14-21.15 | 18.43-18.80 | 18.41 | 23.88-23.96 | 0.99 | 0.87 | 1.30 |
| list scroll line | 14.63-14.64 | 13.21-13.26 | 13.15-13.21 | 18.48-18.54 | 1.00 | 0.90 | 1.40 |
| list scroll page | 44.38-44.42 | 39.01-39.26 | 38.96-39.04 | 43.30-43.34 | 1.00 | 0.88 | 1.11 |
| list sort 1000 | 657.1-658.5 | 95.70-96.12 | 94.23-94.73 | 109.9-111.0 | 0.99 | 0.14 | 1.17 |
| button click | 22.33-22.85 | 16.03-16.42 | 16.00-16.32 | 17.95-17.96 | 1.00 | 0.71 | 1.12 |
| string type | 7.75-7.81 | 6.61-6.62 | 6.57-6.58 | 10.77 | 0.99 | 0.84 | 1.64 |
| string backspace | 7.64-7.71 | 6.49-6.55 | 6.44-6.45 | 10.40-10.41 | 0.99 | 0.84 | 1.61 |
| menu open+close | 13.23-13.37 | 13.02-13.11 | 13.03-13.04 | 19.99 | 1.00 | 0.98 | 1.53 |
| iconify | 83.71-83.79 | 75.99-77.65 | 75.57-77.17 | 84.91-85.59 | 0.99 | 0.91 | 1.12 |
| uniconify | 446.2-447.5 | 335.3-337.4 | 334.5-337.6 | 1,037.1-1,037.6 | 1.00 | 0.75 | 3.08 |
| register page | 53.59-53.67 | 42.65-42.70 | 42.60-42.66 | 60.03 | 1.00 | 0.79 | 1.41 |
| popup open+close | 120.0 | 94.74-95.23 | 94.56-94.86 | 139.9-140.0 | 1.00 | 0.79 | 1.48 |
| window resize | 230.1-280.1 | 220.1 | 220.1 | 360.2 | 1.00 | 0.86 | 1.64 |

### A1200/030 (68030 50 MHz, 20 MB fast, OS 3.1)

| operation | MUI 3.8 | lean | trim1 | Zune68 Beta 5 | trim1 / lean | trim1 / MUI 3.8 | Zune68 / trim1 |
|---|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 116.6-141.6 | 10.15-10.50 | 9.76-9.97 | 15.11-15.26 | 0.96 | 0.08 | 1.54 |
| app+windows create | 281.0-299.9 | 188.0-190.6 | 182.9-188.5 | 275.6-278.3 | 0.98 | 0.64 | 1.49 |
| window open+close | 571.9-579.1 | 416.6-420.2 | 415.7-419.3 | 621.8-625.1 | 1.00 | 0.72 | 1.49 |
| full redraw | 147.8-156.9 | 139.1-139.4 | 139.0-139.2 | 161.1-161.4 | 1.00 | 0.91 | 1.16 |
| relayout | 206.9-215.9 | 180.2-181.2 | 180.2-180.8 | 323.4-323.6 | 1.00 | 0.85 | 1.79 |
| slider set | 4.17-4.26 | 2.89 | 2.88-2.90 | 4.71 | 1.00 | 0.68 | 1.63 |
| notify chain | 8.05-8.21 | 5.97-6.00 | 6.00-6.01 | 9.64 | 1.00 | 0.74 | 1.61 |
| cycle set | 2.57 | 2.16-2.17 | 2.17 | 4.94-5.16 | 1.00 | 0.84 | 2.33 |
| radio set | 16.42 | 8.88-8.90 | 8.88-8.93 | 22.97-22.99 | 1.00 | 0.54 | 2.58 |
| text set | 2.55-2.56 | 2.42-2.44 | 2.41-2.43 | 2.45 | 1.00 | 0.95 | 1.01 |
| list insert 1 | 10.61-10.62 | 7.46-7.61 | 7.46-7.64 | 36.13-36.41 | 1.00 | 0.71 | 4.83 |
| list insert 100 | 850.7-852.5 | 63.17-63.48 | 62.40-63.60 | 113.0-113.1 | 0.99 | 0.07 | 1.80 |
| list insert 1000 | 6,840.8-6,845.0 | 256.7-261.7 | 258.2-259.6 | 706.0-708.0 | 1.00 | 0.04 | 2.73 |
| list active | 21.81-21.82 | 19.22-19.61 | 19.31-19.63 | 24.14-24.52 | 1.00 | 0.89 | 1.25 |
| list scroll line | 14.98 | 13.34-13.51 | 13.35-13.57 | 18.81-18.90 | 1.00 | 0.90 | 1.40 |
| list scroll page | 46.13-46.15 | 40.01-40.33 | 40.01-40.68 | 44.40-44.79 | 1.00 | 0.87 | 1.11 |
| list sort 1000 | 658.2-658.9 | 95.45-97.54 | 95.07-95.71 | 114.0-114.6 | 0.99 | 0.14 | 1.20 |
| button click | 23.78-23.98 | 17.07-17.12 | 17.06-17.16 | 19.10-19.53 | 1.00 | 0.72 | 1.13 |
| string type | 8.41-8.42 | 7.09-7.11 | 7.04-7.08 | 11.37-11.45 | 0.99 | 0.84 | 1.62 |
| string backspace | 8.31-8.35 | 6.95-7.04 | 6.93-6.99 | 11.03-11.05 | 0.99 | 0.84 | 1.59 |
| menu open+close | 13.86-13.93 | 13.54-14.24 | 13.65-13.80 | 19.98 | 0.99 | 0.99 | 1.46 |
| iconify | 98.58-98.59 | 79.44-80.66 | 79.37-80.63 | 87.44-87.91 | 1.00 | 0.81 | 1.09 |
| uniconify | 494.9-498.8 | 351.7-353.0 | 350.8-353.2 | 1,067.0-1,087.4 | 1.00 | 0.71 | 3.06 |
| register page | 55.51-55.54 | 44.57-44.77 | 44.65-44.72 | 62.51-63.23 | 1.00 | 0.80 | 1.41 |
| popup open+close | 120.0 | 98.50-98.78 | 98.13-98.74 | 139.8-140.1 | 1.00 | 0.82 | 1.42 |
| window resize | 230.1 | 220.1 | 220.1 | 360.2 | 1.00 | 0.96 | 1.64 |

### A600 (68000, 4 MB fast, OS 3.1)

| operation | MUI 3.8 | lean | trim1 | Zune68 Beta 5 | trim1 / lean | trim1 / MUI 3.8 | Zune68 / trim1 |
|---|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 357.0-364.0 | 37.03-39.68 | 34.76-36.01 | 61.62-61.73 | 0.93 | 0.10 | 1.74 |
| app+windows create | 938.2-942.1 | 712.6-722.9 | 703.6-729.7 | 1,065.7-1,069.2 | 1.00 | 0.76 | 1.49 |
| window open+close | 1,763.8-1,765.5 | 1,385.5-1,394.9 | 1,388.1-1,396.5 | 2,190.0-2,194.9 | 1.00 | 0.79 | 1.57 |
| full redraw | 470.7-475.2 | 442.1-442.3 | 441.8-442.2 | 532.3-532.7 | 1.00 | 0.93 | 1.20 |
| relayout | 698.9-702.6 | 607.5-609.0 | 607.1-608.9 | 1,168.0-1,170.0 | 1.00 | 0.87 | 1.92 |
| slider set | 15.12-15.74 | 10.07-10.12 | 10.09-10.11 | 16.05-16.06 | 1.00 | 0.65 | 1.59 |
| notify chain | 29.38-30.64 | 20.90-20.95 | 20.93-20.95 | 32.83-32.84 | 1.00 | 0.70 | 1.57 |
| cycle set | 9.36 | 7.85-7.89 | 7.83-7.89 | 18.08-19.31 | 1.00 | 0.84 | 2.38 |
| radio set | 60.46-60.52 | 32.31-32.36 | 32.29-32.38 | 82.36-82.38 | 1.00 | 0.53 | 2.55 |
| text set | 9.69 | 9.29-9.33 | 9.30-9.33 | 9.38-9.41 | 1.00 | 0.96 | 1.01 |
| list insert 1 | 32.39-32.49 | 28.96-29.17 | 28.76-29.18 | 125.0-127.2 | 1.00 | 0.89 | 4.35 |
| list insert 100 | 2,746.3-2,747.7 | 223.0-225.4 | 221.6-224.4 | 422.1-424.2 | 1.00 | 0.08 | 1.89 |
| list insert 1000 | 23,958.0-23,962.3 | 1,008.6-1,013.2 | 1,003.9-1,008.3 | 2,752.2-2,779.2 | 1.00 | 0.04 | 2.75 |
| list active | 64.97-65.14 | 60.81-61.49 | 60.76-61.61 | 75.94-78.71 | 1.00 | 0.94 | 1.26 |
| list scroll line | 39.97-40.11 | 37.17-37.47 | 37.07-37.47 | 55.57-57.44 | 1.00 | 0.93 | 1.52 |
| list scroll page | 153.7-153.8 | 134.1-135.3 | 134.2-135.1 | 148.8-150.6 | 1.00 | 0.88 | 1.11 |
| list sort 1000 | 2,494.4-2,496.8 | 380.2-385.1 | 378.9-383.4 | 434.8-434.9 | 1.00 | 0.15 | 1.14 |
| button click | 91.12-92.34 | 60.49-66.62 | 61.40-66.90 | 74.46-74.63 | 1.01 | 0.71 | 1.15 |
| string type | 31.55-31.72 | 26.07-26.14 | 25.86-25.97 | 43.79-43.83 | 0.99 | 0.82 | 1.69 |
| string backspace | 31.20-31.32 | 25.60-25.74 | 25.41-25.64 | 42.41-42.57 | 0.99 | 0.82 | 1.67 |
| menu open+close | 46.49-46.66 | 45.37-50.02 | 46.08-50.63 | 40.13-40.15 | 1.00 | 1.02 | 0.84 |
| iconify | 301.6-303.2 | 229.6-235.1 | 229.6-232.5 | 264.5-266.9 | 1.00 | 0.76 | 1.15 |
| uniconify | 1,535.3-1,541.1 | 1,243.3-1,247.9 | 1,211.2-1,247.9 | 3,987.5-4,023.1 | 0.99 | 0.80 | 3.25 |
| register page | 163.1-163.2 | 145.0-145.5 | 145.2-145.5 | 218.3-220.5 | 1.00 | 0.89 | 1.51 |
| popup open+close | 404.6-404.7 | 339.0-339.3 | 338.9-341.0 | 481.5-486.4 | 1.00 | 0.84 | 1.42 |
| window resize | 620.2-621.6 | 470.1-520.3 | 470.8-519.6 | 791.0-800.3 | 1.00 | 0.78 | 1.65 |

### OS 3.2.1 (A1200, `opbench v21`; MUI 5 at version 19)

| operation | MUI 5 | lean | trim1 | Zune68 Beta 5 | trim1 / lean | trim1 / MUI 5 | Zune68 / trim1 |
|---|---:|---:|---:|---:|---:|---:|---:|
| prefs+app (empty) | 306.5-307.0 | 11.18-11.52 | 10.55-11.53 | 15.55-16.07 | 0.97 | 0.04 | 1.44 |
| app+windows create | 577.8-579.3 | 186.2-196.5 | 186.1-194.9 | 271.6-277.8 | 1.00 | 0.33 | 1.44 |
| window open+close | 1,006.8-1,022.4 | 438.3-442.8 | 440.1-445.5 | 638.5-642.2 | 1.00 | 0.44 | 1.45 |
| full redraw | 206.2-206.6 | 138.9-139.0 | 138.9 | 159.0 | 1.00 | 0.67 | 1.14 |
| relayout | 316.8-317.2 | 181.3-181.7 | 181.1-181.6 | 314.4-318.7 | 1.00 | 0.57 | 1.75 |
| slider set | 6.75-6.76 | 3.02-3.03 | 3.02 | 4.76 | 1.00 | 0.45 | 1.58 |
| notify chain | 12.76-12.77 | 6.22-6.23 | 6.22 | 9.72-9.73 | 1.00 | 0.49 | 1.56 |
| cycle set | 2.73-2.74 | 2.05-2.06 | 2.05-2.06 | 4.05 | 1.00 | 0.75 | 1.97 |
| radio set | 11.86-11.91 | 8.39 | 8.38-8.40 | 21.77 | 1.00 | 0.71 | 2.59 |
| text set | 2.25 | 1.63 | 1.63 | 1.67 | 1.00 | 0.72 | 1.02 |
| list insert 1 | 22.82-22.88 | 7.29-7.30 | 7.29 | 34.21-34.29 | 1.00 | 0.32 | 4.70 |
| list insert 100 | 2,033.1-2,036.7 | 59.26-61.28 | 59.91-61.05 | 106.9-108.9 | 1.00 | 0.03 | 1.78 |
| list insert 1000 | 20,202.1-20,275.0 | 246.8-248.7 | 247.8-248.3 | 683.8-699.4 | 1.00 | 0.01 | 2.79 |
| list active | 39.08-39.24 | 18.58-18.69 | 18.43-18.66 | 21.58-21.59 | 0.99 | 0.47 | 1.16 |
| list scroll line | 21.11-21.14 | 14.56-14.61 | 14.56-14.59 | 16.80-16.82 | 1.00 | 0.69 | 1.15 |
| list scroll page | 56.86-56.92 | 39.79-39.92 | 39.76-39.78 | 41.29-41.39 | 1.00 | 0.70 | 1.04 |
| list sort 1000 | 2,306.7-2,307.6 | 139.1-140.2 | 137.1-138.3 | 152.4-154.4 | 0.98 | 0.06 | 1.12 |
| button click | 21.24-21.29 | 16.15-16.36 | 16.14-16.23 | 18.12-18.22 | 1.00 | 0.76 | 1.12 |
| string type | 15.20-15.31 | 7.44-7.46 | 7.41-7.45 | 10.81 | 1.00 | 0.49 | 1.45 |
| string backspace | 14.63-14.72 | 7.35-7.42 | 7.28-7.35 | 10.43-10.44 | 0.99 | 0.50 | 1.43 |
| menu open+close | 25.57-26.02 | 19.93-19.99 | 19.92-19.99 | 19.98-19.99 | 1.00 | 0.77 | 1.00 |
| iconify | 195.9-197.3 | 96.07-99.44 | 96.30-97.93 | 103.6-104.1 | 1.00 | 0.49 | 1.07 |
| uniconify | 1,219.5-1,219.6 | 370.2-372.5 | 370.1-373.6 | 1,063.7-1,084.7 | 1.00 | 0.30 | 2.89 |
| register page | 88.49-88.68 | 44.95-45.03 | 44.91-44.94 | 54.27-54.30 | 1.00 | 0.51 | 1.21 |
| popup open+close | 532.6-540.7 | 103.0-104.0 | 102.9-103.7 | 139.6-139.9 | 1.00 | 0.19 | 1.35 |
| window resize | 290.1 | 170.0-170.1 | 170.0-170.1 | 240.1 | 1.00 | 0.59 | 1.41 |
| text shorten set | - | 3.10-3.11 | 3.11-3.12 | 3.23 | 1.00 | - | 1.04 |

Single instances above lean's highest by more than 0.5 %: A1200 list insert 1 (7.33 against 7.24, the
other three 7.17-7.18), A1200/030 list scroll page (40.68 against 40.33, the others 40.01-40.10), A600
app+windows create, menu open+close, popup open+close and button click (bimodal on both builds, as in
the rounds before), OS 3.2.1 window open+close (445.5 against 442.8, the others 440.1-443.0): the
library's place in memory, one percent between two instances of the same build (`opt1`). The means:
trim1/lean 0.93-1.01 everywhere, none above 1.00 but the A600's button click (1.01, bimodal).

### Memory (MUI-Demo cold, `measure.sh` twice each, FS-UAE, bytes)

| | first program | each further |
|---|---:|---:|
| trim1, A1200 OS 3.1 + 4 MB fast | 423,536 | 187,272-187,336 |
| lean, the same | 426,984-427,032 | 188,544-188,608 |
| trim1, A1200/030 | 423,536 | 187,336 |
| lean, A1200/030 | 427,032 | 188,544-188,608 |
| MUI 3.8, A1200/030 | 460,248-460,296 | 210,632-218,824 |
| Zune68 Beta 5, A1200/030 | 672,304 | 267,872 |

(`libonly.sh` did not run: its helper `$ZWORK/libs` is gone.) Raw output: the round's work folder
(outside the repository; deleted after the round).

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
  `scrollpix.c`: the window's checksums after page switches and list scrolls;
  `tests/lean/framepix.c` (every frame type, raised and recessed, at many sizes, thick and thin)
  `imgpix.c` (every built-in image 1:0 to 1:23) and `rectpix.c` (Rectangle's bars and bar titles in
  four title colours, thick and thin): pixel checksums to compare builds.
- `tests/native/cfgshare.c` (the shared settings file: applications made before and after a change
  of `ENV:OpenMUI/global.prefs`, SetConfigItem, own files), `proppix.c` (scrollbars' pixels after
  their values change), `instsizes.c` (every class's instance size, from any muimaster.library),
  `idcmpprobe.c` (a window's IDCMP classes and flags in six situations; `hold N`: WFLG_RMBTRAP, the
  pointer and the windows on the screen four times a second while the pointer is moved from
  outside), `menugeo.c` (another program's menu strip as Intuition has it).
- `tools/lean/mtreport2.py DUMP LIB cats|files|big|N`, `tools/lean/classsizes.py
  BUILD`, `tools/lean/flexcat-sd.py`.
