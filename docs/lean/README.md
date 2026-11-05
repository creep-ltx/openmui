# Zune68, lean edition

Branch `lean`: Zune68 with the memory diet of Zune68k ported, so that it
reports MUI 5 (21.227) and stays close to MUI 3.8 in memory. The goal:
stock OS 3.1 works (68000 build the default, 68020 too), every feature
programs use is kept, Zune68's behaviour is kept (smart refresh, as MUI
3.8), and only MUI 5-only parts and rarely used classes become on-demand.

MUI-Demo, cold, OS 3.1 + 4 MB fast (FS-UAE, 68000 build):

| | library file | library alone | first program | each further | first window |
|---|---|---|---|---|---|
| MUI 3.8 | 166 KB | 238 KB | 460 KB | 211-219 KB | 1.48-1.5 s |
| Zune68 main | 429 KB | 535 KB | 832 KB | 334 KB | 1.9-2.0 s |
| lean | 299 KB (+180 KB in 40 class files) | 310 KB | 519 KB | 223 KB | 1.5 s |

(lean measured 2026-11-05 with GCC 16.2; the other rows 2026-11-04.)

perfbench (68EC020, same session): lean is faster than MUI 3.8 in seven of
eight batches; "list active" is 4% slower (Zune68 main 7%).

Details: [measurements.md](measurements.md), the class split:
[class-split.md](class-split.md), GCC 16.2 against GCC 6.5 (16.2 is 6 KB
smaller in memory and 1-3% faster in some batches):
[compiler.md](compiler.md).

## What differs from upstream

One commit per change, each meant to be able to stand as its own pull
request. Checked with `git cherry-pick` onto main: the commits with "-"
apply alone, the others on top of what "depends" names.

| commit | change | saving (MUI-Demo, OS 3.1) | depends |
|---|---|---|---|
| be74ee7d `muimaster: open rexxsyslib.library v36 so it starts on stock OS 3.1` | 3.1 has rexxsyslib 36; asking for 37 made the library fail to open | makes it open at all | - |
| 294fa0af `muimaster: open optional libraries on first use` | asl, commodities, rexxsyslib, datatypes, iffparse, diskfont, icon, workbench and the IEEE double libraries opened when a feature needs them (`lazylib.h`); a missing one fails only its feature; fonts in memory come from OpenFont | library alone -104 KB, first program -79 KB | be74ee7d (the rexxsyslib line) |
| 7f31a15e `muimaster: smaller per-application and per-list memory` | window positions on demand (was 6 KB per application), one block per notification, List title entry outside the pool, 1 KB list and window pools, list columns reserved as needed (at least 16) instead of 64 | each further -33 KB | 294fa0af (one line in notify.c) |
| 2475c0fd `Area: make the event and timer nodes at the first setup` | event/timer nodes, click position and context menu state in a block made at first setup, kept until dispose; Area 238 -> 136 bytes | each further -35 KB | - |
| f949dc3d `muimaster: MUI 3.8 behaviour that TheBar and YAM rely on` | MUIA_Group_Horiz settable after OM_NEW, MUIA_KnowsDisabled, draw mode restored after JAM2 drawing, Numericbutton formatting codes not drawn | none (fixes TheBar demo2/demo3, YAM labels) | - |
| cdecd1a7 `muimaster: rarely used and MUI 5-only classes as files loaded on demand` | 35 classes as `LIBS:Zune/<Name>.mui`, made through the built-in path; release and installer carry them | library file -109 KB, library alone -102 KB, first program -85 KB | 294fa0af (leave out its tests/lean/scripts/putlib.sh, lean tooling) |
| cd4f882e `Build: turn off bebbo GCC 6.5's auto-increment pass` | bebbo's `-fbbb` pass "i" made `(a2)+` and left later offsets from a2 unchanged: Numeric's OM_SET compared the wrong fields and relaid out the parent group on every set; `-fbbb=abcefhlmnprsz0` when the compiler has `-fbbb` | no memory change (+508 bytes); slider set 23.8 -> 2.75 ms, perfbench notify chain 31.3 -> 0.60 s | - |
| 3bde59da `Group: keep the children in the group, not in a Family object` | the child list in Group's instance data, Family's methods on it directly; no Family object per group | each further -5.5 KB, first program -5.3 KB (116 objects fewer), create+dispose -10% | - |
| fbba14ed `muimaster: 64-by-32-bit division of its own instead of libgcc's` | ZuneDiv64 for Numeric, Prop and String's 64-bit integers; libgcc's __udivdi3/__umoddi3 (3.2 KB) no longer linked | library file -3.5 KB, library alone and first program -3.3 KB | - |
| 6e32efa3 `Configdata: the screen's font by default, as MUI 3.8` | Tiny/Title/Big default to the screen font (were helvetica/9 and /15): no diskfont.library and no font from disk without saved prefs. **Visible**: frame titles in the screen font, as with MUI 3.8 | first program -18 KB, first window -0.05..0.1 s | - |
| 140d5b1f `List: make the horizontal scroller at the first setup` | MUI 3.9's horizontal list scroller (~1.3 KB of objects per list) made at the list's first setup instead of with the list | first program and each further -17.4 KB, first window -0.15..0.2 s | - |
| 08e1f52e `muimaster: Dtpic as a class file` | Dtpic in `LIBS:Zune/Dtpic.mui` (MUI 3.8 also ships it as a file); `zune_cf_refresh()` takes the library bases over again after a window opened cybergraphics.library | library file -4.9 KB (Dtpic.mui 6.3 KB), library alone and first program -4.6 KB | cdecd1a7 |
| 1978585e `muimaster: never load a real MUI .mui as an external class` | a `.mui` Zune has not built in (Pixmap.mui) is opened only if its id string says Zune68, read from its Resident tag before any of its code runs; real MUI 4/5's own Pixmap.mui, Popfrimage.mui... were loaded into Zune and crashed. Upstream candidate (cherry-picks onto main and builds) | none (+0.4 KB code) | - |
| 3170592b `muimaster: a private copy of each class file per library` | each library base LoadSegs its own copy of a class file; one shared copy served only the first of muimaster.library and zunemaster.library, so Zune prefs (zunemaster, with custom classes on muimaster) lost Balance.mui and opened no window | first program +136 bytes, each further 0 | cdecd1a7, 1978585e |
| e078b8d4 `Installer: classes into the first LIBS: directory, Pixmap.mui to LIBS:Zune` | Libs/MUI to `<first LIBS: dir>/MUI`, not MUI 4/5's own MUI:Libs/mui; Pixmap.mui with the class files in LIBS:Zune | none | cdecd1a7 |

The tools and reports (`tests/lean/`, `tools/lean/`, `docs/lean/`) are
for measuring only; they are not part of any of these changes.

## Audit fixes (2026-11-05)

A code review of the lean changes and of the unchanged Zune code, one
commit per area. Most are upstream bugs (every commit but the first and
the class-file check applies to main as well). Checked in FS-UAE: a clean
build, fullcheck (19 demos + MUI-Demo's 8 sub-windows, no alert, memory as
before) and the Popasl case below on OS 3.1 and 3.2.1.

| commit | fix |
|---|---|
| 60254ec4 Dataspace | ReadIFF/WriteIFF with the program's own IFF handle called iffparse through a NULL base (lean's lazy libraries) |
| 532b1ef9 List | a failed MUIA_List_Format no longer leaves the list without a format (crash at the next redraw); entries sized for the allocated columns; list images cleaned up at Cleanup (leak per reopen) and DeleteImage safe before setup; StringArray rows read only the columns they have (strlen() past short arrays); Remove_Selected in one pass; no ZText allocation per measured column |
| 1738c596, 4f22a12f Popasl | quitting with a requester open freed it under the requester's process; OS 3.0/3.1's asl.library has no AbortAslRequest (V44), Cleanup called it anyway. `tests/lean/popaslquit.c`: old library on 3.1 resets the machine, fixed one waits for Cancel (3.1) or aborts (3.2.1) |
| 1f9333ae Application | failed timer allocation left a stale pointer for RemInputHandler; timers a class forgot are aborted at dispose |
| ec22e098 Cycle | popup texts leaked when the popup window did not open |
| beee6061 Dtpic, Bitmap, brushes | ARGB read in little-endian order (wrong colours on RTG); masked blits through MyBltMaskBitMapRastPort (interleaved bitmaps); alpha brushes on planar screens read the picture on every draw and drew nothing |
| cd8c2341 Floattext, Dirlist, Pixmap | quiet list while typesetting; one notification per directory instead of four per file; CLUT matches remembered, rows written with WritePixelLine8 |
| cfb5ddda String, Slider | BufferPos clamped; new buffer starts empty; no strlen(NULL) |
| cf83a36e class files | the generator refuses register-argument functions in the jump table (the jumps use a0) |
| 1ceed3ab lean tests | REALDIR instead of a test machine's volume name |
| 78f0145e Group | 2-D groups keep their row and column arrays between AskMinMax passes (freed and allocated again every pass) |
| 00739c0a List | a cursor move clips only when a row needs it (on OS 3.1 a clip costs ~1.5 ms): cursor moves inside the view -33% |
| 8a36e810 Scrollbar | the scroller position goes to the prop only, not to both arrow buttons as well |
| 4b8072cc Notify | 24-byte nodes; MUIV_EveryTime messages with up to 7 parameters built on the stack, not kept twice: -4.4 KB per MUI-Demo |
| 92475b94 Image specs | built-in image and pattern spec strings shared, not copied per object |
| e2205d20 class files | Register, Bitmap, Bodychunk, ChunkyImage as class files: library -11.8 KB |
| 06caea05 Group | a virtual group's state in a block of its own: Group's data 168 -> 96 bytes (MUI 3.8: 42) |
| 329644e9 Area | setup-only and rarely set fields out of every object: Area's data 136 -> 114 bytes (MUI 3.8: 108); with Group -14.4 KB per MUI-Demo window |

Not changed, and why: Window's explicit cycle chain sorts in O(n^2) per added object (only programs that
call MUIM_Window_SetCycleChain, short chains); a `.mui` in LIBS:MUI is
read twice the first time (once to check its id, once by OpenLibrary);
Floattext still draws twice on a width change (old lines, then the
retypeset ones: typesetting first and drawing once left the text empty,
reverted); two programs starting at
the same moment can get the same ARexx port name.

## Settings and the image chooser (2026-11-05)

| commit | change |
|---|---|
| 63da03c4 Configdata | an application's own settings file is laid over the global settings instead of replacing them, and quitting with remembered windows writes only the window positions into it (measured on MUI 5: a 102-byte positions-only file, global settings kept). Before, the first quit froze an application's settings: the Zune prefs program ignored a saved Cycle image |
| dee49cc9 Imageadjust | the External page is a tree of the image drawers (double-click opens and closes a pack) with a preview of every image, names aligned; the chosen image's pack is open and the image centred when the page opens |
| dce5fcc2 Imageadjust | no preview for pictures larger than a gadget image (64x40, from the IFF or PNG header): one 96x48 logo made every row 50 px tall on the A1200; Workbench icons not listed |
| c86d5731 muimaster | external classes as MUI: each opened once and kept while used (MUI_GetClass references, objects); objects hold no open, so DisposeObject() instead of MUI_DisposeObject() leaks nothing; unused ones closed when a program closes the library. A1200: Rawimage.mcc had 96 opens and nothing could leave memory; now all leave on a flush |
| e5f03897 Coloradjust | an object behind "cond ? Child : TAG_IGNORE" was made anyway: one Rectangle lost per image chooser opened |
| afd238c0 Zune prefs | the System page's two classes deleted at the end; preference module libraries always closed |

## `make check` (2026-11-05)

The project's own checks (`tests/check_*.py`, linked 68000 code run in
amitools/vamos) need the Python that comes with the toolchain:
`make check PYTHON=~/opt/amiga-gcc16/libexec/amitools/bin/python` (the
system Python's amitools is too old: `Machine.from_name`). Until
2026-11-05 they had not been run on this branch; they then failed in 8
places, all where the checks fake an object or the library base at fixed
byte offsets that the lean edition moved (Area 238 -> 136 bytes, the
Application's window positions, the library base's new fields), or test a
behaviour the lean edition changed on purpose (screen font by default, one
block per notification, graphics.library asked before diskfont), plus two
from the audit fixes (Pixmap's row drawing, the BMA_FLAGS query). The
checks were updated, the library base keeps its class list and default
pens last as the checks expect, and with class files on, `check-core`
runs on a second build with every class inside the library
(`$(BUILD)/check-builtin`). Upstream main passes all of them unchanged.

## Left open

- **Library code**: 315 KB against MUI 3.8's 166 KB. Compiler options do
  not close it (measured: `-fomit-frame-pointer` -2.7 KB, other `-f`
  switches within 0.3 KB, alignment none, `LTO=1` with bebbo's GCC 6.5
  -3.6 KB and it cannot link the class files: no `nm` plugin, fat LTO
  objects make the linker write nothing). What is left is Zune's own
  code: List 30 KB, Window 29 KB, String 21 KB, Group 20 KB, Area 16 KB,
  Application 15 KB, frames 11 KB, text engine 9 KB. The next movable
  parts are not classes (custom frames and datatype pictures 18 KB,
  context menus 8 KB, pointer types 2.5 KB) and need a loader for
  non-class code first.
- **First window** 1.72-1.76 s against MUI 3.8's 1.48 s. Loading is not
  the difference: the library opens in 57 ms cold (MUI 3.8: 209 ms with
  muigfx/muilowlevel), a small application and its window take the same
  time as on MUI 3.8; a second MUI-Demo with everything loaded takes 1.47
  s against 1.11 s. Where the rest goes in MUI-Demo is not found yet.
- **Per program** 243 KB against 211-219 KB: 17 KB is smart refresh
  backing store, the rest is spread (notifications 13 KB, list pools
  14 KB of 1 KB puddles, MUI's own default is 2 KB, Text contents and
  text engine 8.5 KB, Group's virtual-only fields ~4 KB).
- NListtree-Demo opens no window (Zune68 main too).

- Tested in FS-UAE only, not on real hardware; Dtpic not on an RTG screen.
