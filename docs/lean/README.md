# Zune68, lean edition

> **Renamed 2026-11-11: this edition is now OpenMUI** (first beta
> 0.1b1), earlier test builds were "Zune68k lean". OpenMUI is not MUI and
> is not affiliated with MUI's authors; MUI is the product of Stefan
> Stuntz. The notes below keep the names of their time ("Zune", "Zune68k",
> "lean") where they describe what was done then. The rename changed
> texts only: the library files `muimaster.library` and
> `zunemaster.library`, the version numbers, `LIBS:Zune`, `ENV:Zune` and
> `ENVARC:Zune` are as they were; the prefs program is `SYS:Prefs/OpenMUI`,
> the installer drawer `SYS:OpenMUI` (was `SYS:Zune68k`), the switch tool
> `OpenMUISwitch` (was `ZuneSwitch`, source `workbench/tools/openmuiswitch`).
> The archive is `OpenMUI-0.1b1-DATE-amigaos3-m68k.lha`. See the
> [root README](../../README.md).

Branch `lean`: Zune68 with the memory diet of Zune68k ported, so that it
reports MUI 5 (21.227) and stays close to MUI 3.8 in memory. The goal:
stock OS 3.1 works (68000 build the default, 68020 too), every feature
programs use is kept, Zune68's behaviour is kept (smart refresh, as MUI
3.8), and only MUI 5-only parts and rarely used classes become on-demand.
Each program sees the MUI it asks for: MUI 3 programs 19.35 (MUI 3.8),
MUI 4 programs 20.x, MUI 5 programs 21.227, unless Zune prefs' "Behave
as" says otherwise ([below](#each-program-gets-the-mui-it-asked-for-2026-11-06-phase-8)).

MUI-Demo, cold, OS 3.1 + 4 MB fast (FS-UAE, 68000 build):

| | library file | library alone | first program | each further | first window |
|---|---|---|---|---|---|
| MUI 3.8 | 166 KB | 238 KB | 460 KB | 211-219 KB | 1.48-1.5 s |
| Zune68 main | 429 KB | 535 KB | 832 KB | 334 KB | 1.9-2.0 s |
| lean | 277 KB (+215 KB in 44 class and module files) | 289 KB | 497 KB | 222 KB | 1.5 s |
| smaller | 262 KB (+236 KB in 47 class and module files) | 275 KB | 477 KB | 212 KB | 1.4 s |
| memory | 263 KB (+236 KB in 47 class and module files) | 276 KB | 468 KB | 203 KB | 1.4 s |
| realmem | 263 KB (+236 KB in 47 class and module files) | 276 KB | 465 KB | 200 KB | 1.4 s |
| mui45 | 265 KB (+ class and module files; Hyperlink.mcc, Settings.mui, Settingsgroup.mui new) | 278 KB | 467 KB | 200 KB | 1.4-1.5 s |
| smaller2 | 261 KB (+ Zmui45, Zprefs, Crawling as files) | 274 KB | 463 KB | 200 KB | 1.4 s |

(lean measured 2026-11-05 with GCC 16.2, after the modules below; smaller
2026-11-06 (branch `smaller`, [below](#a-smaller-library-2026-11-06-phase-3));
memory 2026-11-06 (branch `memory`, [below](#memory-per-program-2026-11-06-phase-4):
479,064 / 207,736 bytes, MUI 3.8 the same session 460,296 / 210,696-218,824;
this table is in KiB); realmem 2026-11-06 (branch `realmem`,
[below](#real-programs-memory-against-mui-38-2026-11-06-phase-4-follow-up):
476,368 / 204,456-204,504 bytes),
where lean measured the same session gave 289 / 490 / 212 KB; the other
rows 2026-11-04; mui45 2026-11-10, branch `mui45`,
[below](#mui-45-programs-a-wider-test-set-and-what-they-missed-2026-11-10-phase-6):
271,132-byte file, 284,536 / 478,016 / 204,520-204,584 bytes; `lean` the same session 285,136 /
478,616 / 204,520-204,584; smaller2 2026-11-11,
[below](#a-smaller-library-second-round-2026-11-11-phase-3): 267,400-byte file, 280,808 /
474,312 / 204,520-204,584 bytes, `lean` the same session 271,676 / 285,128 / 478,560 /
204,520-204,584.)

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

## MUI 3.8's window gadgets and stack (2026-11-05)

| commit | change |
|---|---|
| 155c5539, d7c0325e, 89b40e94 Window, Zune prefs | the extra title bar gadgets as MUI 3.8 has them: Popup (window menu: Iconify, Snapshot, Unsnapshot, Settings..., About MUI...), Prefs, Snapshot (click stores position and size in ENV: and ENVARC:, double-click forgets), Iconify; 23 px boxes the height of the title bar, left of zoom and depth, in Intuition 3.1's style; no Snapshot without a window ID, none without a depth gadget; all off by default, switched on the Windows page. Written clean-room from a description of MUI 3.8's behaviour (studied running, not disassembled); the drawings are our own. Library +808 bytes |
| fc7643d2 muimaster | runs on a 4 KB stack: MUI-Demo crashed after its Backfill window (Zune used 9.3 KB of stack, MUI 3.8 3.6 KB). Group's draw no longer keeps 1.1 KB of buffers per nesting level, big rarely used helpers are no longer inlined into the dispatchers (9.3 -> 4.9 KB), and with less than 6000 bytes left an application's input methods and window opening run on an 8 KB spare stack kept until the application is disposed. All demos on a 4 KB stack, no crash, memory as before |

## Modules: non-class code loaded on demand (2026-11-05)

Rarely used parts of the library that are not classes now live in files of
their own too, `LIBS:Zune/Z<name>.mui`, loaded through the class-file loader
the first time a program needs them (`modules.h`, `modules.c`,
`classes/z*.c`). Each module is a private class (`Zmenu.zmod`, ...: names
only Zune asks for, never looked for as external classes) whose
dispatcher, called with no object, returns a table of the module's
functions. The library keeps every function under its old name
(`modules.c`); each looks the class up (`ZUNE_GetBuiltinClass`), calls the
module's function, and gives the class reference back, so a module stays in
memory while it runs and leaves with the library's expunge, as class files
do. The module sources include the original files (`menu.c`,
`dragndrop.c`, ...) with their exported names renamed; `datatypescache.c`,
`requesters.c` and `mui_request.c` are built twice (library half, module
half). `CLASSFILES=` builds the modules into the library, where the same
lookup finds them among the built-in classes (`make check` runs on that
build).

| module | what moved | library code saved | glue in the library | file |
|---|---|---|---|---|
| Zmenu.mui | `menu.c`: Zune's own menus, used for context menus and `MUIM_Menustrip_Popup` | 7,720 | 346 (4%) | 9,596 |
| Zpicture.mui | `datatypescache.c` minus the image containers, the masked blit and `dt_width`/`dt_height`: datatype pictures for brush (`4:`) and bitmap (`5:`) image specs, menu images, custom frames (`frame.config`) | 6,116 | 526 (9%) | 9,568 |
| Zdragdrop.mui | `dragndrop.c`: the drag image of drag and drop | 3,836 | 560 (15%) | 5,536 |
| Zrequest.mui | `requesters.c` (`MUI_AllocAslRequest`, `MUI_AslRequest`, `MUI_FreeAslRequest`) and `MUI_RequestA`'s requester window (`mui_request.c`; `MUI_RequestA` without an application stays: EasyRequest) | 4,688 | 276 (6%) | 6,888 |
| Zgradient.mui | `imspec_gradientdraw.c`: gradients on a true colour screen (the planar start-pen fill stays in the library and needs no module) | 3,072 | 378 (12%) | 5,112 |
| shared | lookup (`zune_module_get`), names, failure flags, 40 bytes of jump table | | ~250 | |

Library file 301,904 -> 276,796 bytes, code (text) 282,900 -> 259,448.
MUI-Demo, cold, OS 3.1 + 4 MB fast (FS-UAE, 68000 build, GCC 16.2,
`measure.sh` twice, `libonly.sh`): library alone 312.2 -> 288.7 KB, first
program 520.7 -> 497.2 KB, each further 222.3 -> 222.3 KB, first window
1.46-1.48 -> 1.46 s. MUI-Demo loads none of the modules.

- **If a module cannot be loaded** (file missing, no memory, a caller that
  is not a process the first time), its feature fails as on low memory: no
  context menu, no drag, `MUI_RequestA` returns 0, `MUI_AllocAslRequest`
  NULL, no picture, gradients as the start-pen fill. A failure is not
  retried while the library is in memory (a missing file would otherwise
  be looked for on every draw), except one from a task that is not a
  process or with `ERROR_NO_FREE_STORE`. Tested with the five files
  deleted: no crash, the rest of every program as before.
- **Checks:** `make check` (`check_resources.py` calls the picture module's
  `zmod_load_custom_frame` and `zmod_put_on_rastport_tiled` directly);
  fullcheck (19 demos + MUI-Demo's 8 sub-windows, no alert; lost bytes
  MUI-Demo 128, AppWindow 112, Layout 24, Virtual -24, rest 0); `real.sh`
  (NList, NListtree, TextEditor, TheBar 1-10, YAM) with the same windows,
  lost bytes and pixel-identical screenshots as lean before; by hand,
  pixel-identical to lean before: MUI-Demo's string context menu with its
  Edit submenu, the About requester (`MUI_RequestA`), Popup's file
  requester, a brush, a bitmap background and a gradient (`tests/lean/pictest.c`), NList
  drag-sort (drag image, drop) and its title context menu, TextEditor
  selection by dragging, TheBar demo1's buttons. Wasabi's mouse moves carry
  no button qualifier, so Area's drag never starts from them; the drags
  were done with `tests/lean/mdrag.c`, which sends the left button and
  the moves with `IEQUALIFIER_LEFTBUTTON`. Gradients were drawn on AGA only
  (start-pen fill, no module): not tested on an RTG screen.
- **Not moved:** `pointer.c` (2.5 KB): String sets a text pointer on every
  string gadget, so the module would load as soon as the mouse crosses one
  in almost any program, and cost a file's overhead on top. Inside the big
  classes (Window's title gadgets and snapshot ~3.5 KB, off by default;
  List's edit and drag parts ~2.4 KB; String's context menu 0.35 KB) there
  is no clean cut: the code works on the classes' private instance data,
  which would have to move into shared headers, for a few KB.
  (2026-11-06: the title gadgets and List's edit and drag parts were
  moved after all, through small interfaces and `classes/list_private.h`:
  "A smaller library" below. The snapshot stays: positions are remembered
  on exit by default.)

## MUI 3.8's sizes (2026-11-05)

| commit | change |
|---|---|
| 1d7331d7 Window | windows larger than the screen are fitted in MUI 3.8's three stages (helvetica 9 and 1 pixel spacing; no inner frame spacing; topaz 8), measured with a column of buttons: same heights as MUI 3.8 for 17 to 22 buttons, NListtree-Demo now opens (it never did on Zune). MUIA_Window_Width/Height read back the whole window as on MUI 3.8 (Zune returned the inner size) |
| 1fa6f196 Configdata | MUI 3.8's defaults, read from its settings program: frames with 1 pixel of top/bottom inner spacing (were 2), groups 6/3 (were 4/4), window spacing top/bottom 3, group titles above, cycle popup background shine, positions remembered on exit, bubble help after 2.0 s / 0.3 s from the settings (Zune ignored them). A column of buttons now gives MUI 3.8's window heights exactly. Redraw stays "without clear" (same pixels, drawn once) |
| b3d303f3 Frames | MUI 3.8's "Frame thickness" (auto/thick/thin; auto, the default, is thick when the screen is at least twice as wide as tall; decided per window at setup through MUIMRI_THINFRAMES). Thick frames have 2-pixel left and right lines that count in the size: types 1 and 2 are 2 pixels wider, 3 and 9 4 pixels, 4 keeps its size and fills its gap ring. Setting `MUICFG_Frame_Thickness` (0x50b, Zune's own id, MUI's is unknown; old files give auto), cycle on the prefs Windows page. With the next two rows: library code +1.6 KB (file 276,900 -> 278,632 bytes), no memory per object |
| 0898009f Frames | type 8 (round thin border) 3 pixels wide as in MUI 3.8 (was 4), with MUI 3.8's corner pixels |
| d994c0d3 Text | the default width is the minimum, as MUI 3.8: Text added the frame and inner spacing a second time. SimpleButton "B" 16, "Abc"/"WWW"/"iii" 32, "_Ok" 24, "Button" 56, "ButtonText" 88 on a thick screen (2 less on a thin one), exactly MUI 3.8's; two buttons in a column 72x54 as MUI 3.8 (were 76x54) |

How it was checked (FS-UAE, OS 3.1 + 4 MB fast, 68000 build, GCC 16.2), from a
black-box description of MUI 3.8 (measured on MUI 3.8, not disassembled):

- `tests/lean/framethick.c` opens a 16-colour screen with a pen of its own
  for every MUI pen and draws a text button in every frame type, raised and
  recessed; `tools/lean/framecmp.py` compares its pixels with MUI 3.8's
  maps. Types 0-4, 8 and 9 are pixel for pixel MUI 3.8's (its 9 is MUI
  3.8's 6), raised and recessed, on 640x256 and on 640x512 interlaced, with
  auto, thick and thin (auto: thick on 640x256, thin on 640x512). Button
  widths as in the table above in all six cases.
- **Frame type numbers 5-10 are MUI 4/5's, not MUI 3.8's**, and stay so.
  MUI 3.8's 5 is Zune's 6 raised, its 6 Zune's 9, its 7 Zune's 10; its 9
  and 10 differ from Zune's 7 and 5 by a few corner pixels and the length
  of two lines. A first try that drew 6 as MUI 3.8 does took the frames off
  every TheBar button: TheBar's default button frame is "602222" (MUI 4/5's
  xen look; MUI 3.8's 6 is in the half pens, which on a 4-colour Workbench
  are the background). MUI 3.8 takes no frame spec in MUIA_Frame, so only
  MUI 4/5 programs (and Zune's own settings files) ever name these numbers.
- `make check` passes unchanged. fullcheck: 19 demos and MUI-Demo's 8
  sub-windows (default button rows) open and close, no alert, memory as
  before (each program writes its positions file once: 152-264 bytes,
  MUI-Demo 608). `real.sh`: NList, NListtree, TextEditor, TheBar 1-10 and
  YAM open and behave as before; screenshots differ only by the wider
  frames and narrower buttons (TheBar's own bars pixel-identical).
- Zune prefs: the cycle reads and writes the setting; "thin" + Use makes the
  next window thin (two buttons 70x54), "auto" again 72x54.
- Not done: the two small preview boxes MUI 3.8 shows beside each choice.

## Crashes found by the real-program suite (2026-11-06)

Four programs that reset or hung on Zune and not on MUI 3.8
([tests/lean/real-programs.md](../../tests/lean/real-programs.md)). Each
cause was found on the machine, not guessed: a trap recorder (the exception
vectors moved to a table of our own, frame, registers and stack kept in
memory that survives the reboot, exec's Alert() recorded too), the saved
context of a looping task sampled, and trace builds printing setup,
cleanup and window opening. MUI 3.8 and MUI 5 were measured by calling them
from test programs (not read from their code).

| commit | fix |
|---|---|
| 31b35e04 muimaster | MUI 3.8's whole jump table (down to LVO -306, lib_NegSize 308); Zune's ended at -240. AmIRC 3.6.2 calls the private -264 at start: the call went into whatever lay below the library base, a guru (8000000B, 80000008 or 80000004 by what was there), which on OS 3.1 reboots. On MUI 3.8 and MUI 5, -264 returns one global object (a Semaphore subclass, MUI 5's Applist.mui) whose private method 0x8042e50f finds an application by an attribute's string (AmIRC: a free ARexx base name); Zune now has both, the other private slots return 0. Zune68 main had the same hole |
| 907955da Dtpic | NewDTObject() and GetDTAttrs() called through the global DataTypesBase (the varargs forms of the inline headers are made at #include time), not the base Dtpic opens: NULL in Dtpic.mui and in the library until something else opens datatypes. HomeBank 3.1 jumped to address -48 at start (guru 8000000B, reset). Now only NewDTObjectA/GetDTAttrsA |
| 6b2c626f Group | a child's failed MUIM_Setup left the group's own Area part set up; Window's dispose then sent the group MUIM_Cleanup, passed on to children never set up. MUIMine 1.2 (its mine field window does not fit, as on MUI 3.8) reset the machine when it quit. The group now undoes its own setup too. Upstream bug, harmless there until the next row's change |
| 1aab8260 Area | MUIM_Cleanup does nothing on an object that is not set up: since mad_Live is made at the first setup, a Cleanup of a never set up object read its handler nodes and selected-state image through NULL, wrote into the exception vectors (vector 21) and freed a pointer read from them (exec alert 0100000F) |
| ec9d8706 Menuitem | a Menuitem with items directly in a Menustrip is a menu, as on MUI 3.8 (where every object of the strip is a menu). Zune (AROS, 2015) made it an item, so Archiver 2.3.24's NewMenu array began with an item and gadtools' LayoutMenusA() never returned: no window, task busy. Archiver's menu strip now dumps identical to MUI 3.8's. A childless Menuitem after a menu stays an item (MUI 3.8: an empty menu; MUI 5 not checked) |

Checked: `make check` passes after each commit; the whole MUI 3.x suite on
Zune with all five gives 32 of 36 lines as before them and the 4 that differ
are AmIRC, HomeBank, MUIMine and Archiver, now as on MUI 3.8 (memory kept by a
second run: -56, -4232, 0, -56 bytes).

Not fixed: IBrowse 3.0a and 2.5.9 "freeze" at a later run because the
emulator process itself dies (FS-UAE 3.2.35 aborts with "malloc(): unaligned
tcache chunk detected" in its filesystem thread: host heap corruption), 4 of
4 times on Zune, 0 of 3 on MUI 3.8 (two runs each). IBrowse's DOS packets to
the drive are the same on both up to the crash (logged with a PutMsg patch);
what in Zune sets it off is not found. IBrowse 2.5.9's "Couldn't open
initial window" is the window fitting: its main window's minimum height is
361-370 pixels in every fitting stage, the screen 256.
| 08ed5791 muimaster | "Behave as: MUI 3.8 / MUI 5" in Zune prefs (System page), MUI 3.8 by default: the library reports 19.35 (MUI 3.8) unless ENV:Zune/Identity says MUI5 (21.227, as Zune68), read when the library loads. Classes choose their code paths by this version: TheBar sized its virtual bars the MUI 5 way (demo9 cut off at 640, as on real MUI 5). Real-program suite with MUI 3.8: no new failure, 14 of 36 programs as on MUI 3.8 (12 before), TheBar demos within 1-2 px of MUI 3.8 (up to 22 before). Programs that require MUI 4/5 need the MUI 5 setting |

## MUI 3.8's behaviour for the suite's differences (2026-11-06, phase 6 part 1)

The differences the real-program suite found
([tests/lean/real-programs.md](../../tests/lean/real-programs.md)), fixed in
order of how many programs they touched. MUI 3.8 was measured black-box only:
test programs that dump every object's minimum, default and maximum size, frame
spacing and font (public MUI_AreaData fields; another program's windows found
through a patch on intuition's NewObjectA), screenshots compared pixel by
pixel, asl.library's calls logged by patching them in the test program's own
task. Nothing of MUI 3.8 was disassembled or read. The test programs are in
`tests/lean/probe/` (`zspy` + `objdump TITLE` dump a running program's windows,
`stage`, `wide`, `tall`, `widef` the fitting stages, `reqtest` MUI_Request,
`ghost` disabled objects, `asltrace` the ASL calls, `tagspy` the tags of new
groups, `pubscr` a tall default public screen to see a window's unfitted sizes).

| commit | change |
|---|---|
| 36fb3297 Area | MUIA_Disabled set bit 31 of a WORD: no object was ever disabled by it (get returned FALSE after set TRUE), nothing was ghosted and disabled buttons could be pressed. Now the WORD's top bit. Ghosting is pixel-identical to MUI 3.8's (buttons, string, cycle, text, list; a disabled group and its children; MUIA_Window_Sleep ghosts nothing on either). MultiRen, TextEditor-Demo's Undo/Redo, AmFTP behind its notice. Upstream bug |
| 50945ad0 Window | fitting a too large window in MUI 3.8's five stages, measured with a row as well as a column of buttons: helvetica 11 (tiny and title 9, big 13); helvetica 9; spacing 2 beside and 1 above; no inner spacing; topaz 8 (fixed font always topaz). Zune had three, which never tried a smaller font with the normal spacing, and spaced 1 beside. Rows of 10-22 and columns of 14-22 buttons: MUI 3.8's sizes, fonts and spacing in every case |
| 8c42267b Window | an application's later windows open in the highest stage any of its windows needed, as on MUI 3.8 (its MUI_Request after a fitted window comes in the small font, also without a reference window). Kept in MUI_GlobalInfo's private first field, no size change |
| 1b1828cc Area | group frame titles: the frame line on the row above the baseline, contents at MAX(font height, baseline - 1 + frame + inner spacing), measured with title fonts of 8 to 13 pixels and in every stage. Topaz 8: 11 (Zune 10). The Textinput demo now needs the small font as on MUI 3.8 (240x256; was 454x256 in topaz) |
| 34667b84 List | at least 40 and by default 100 pixels of content inside the frame (Zune: 40 with the frame, 80): Listviews 66/126 wide as MUI 3.8 |
| 10fb2ac8 Scrollbar | arrow images as high as the font asks (5 in topaz 8, 6 in helvetica 11, 8 in 13), as MUI 3.8 |
| 6e9cb755 Group | a horizontal group's default width counts a weight-0 child's default (MUI 3.8: 112 + 6 + 32), a vertical one keeps the minimum height. AmFTP 561 wide as MUI 3.8 (was 428) |
| 4ad2cb25 Group | a 2-D group fills a partial last row (26 objects in 4 columns: 7 rows), instead of one row of all children: MultiRen's preferences (2168 pixels wide, never opened) open 523x220 as MUI 3.8 |
| 2d7155df Register | titles never widen the group (MUI 3.8 cuts them; Zune's "truncate titles" setting now on by default), the group may always grow, pages 6 pixels from the sides with thick frames (5 thin). Archiver's window 321 wide in the same stage as MUI 3.8 (was 427, and fitted in topaz without a size gadget) |
| 8da6eed7 Slider | MUIA_Group_Horiz FALSE makes a vertical slider (MUI 3.8's autodoc and own example; Archiver's sliders) |
| 8b26c440 Scrollgroup | MUIA_Scrollgroup_FreeHoriz/FreeVert (declared, ignored): an axis that may not move has no scroller. ToDo 313x158 as MUI 3.8 (was 167, a horizontal scroller under its list); one Scrollbar less per such group |
| 9120eecc MUI_RequestA | MUI 3.8's layout: 4 pixels round, 8 between text and buttons, the text a framed Text with 8 inside, buttons sharing the width with 4 pixel gaps and spaces. Five measured requesters have MUI 3.8's object tree and size; WookieChat's "Quit Program?" and Archiver's "Problem!" as MUI 3.8 |
| 62a34d5f List | behaving as MUI 3.8, no MUI 4 horizontal scroller unless the program asks (MUIV_List_HScrollerVisibility_Always); ~1.3 KB less per list. Behaving as MUI 5 unchanged |
| a76f4c5f MUI_AslRequest | behaving as MUI 3.8, asl.library's AllocAslRequest/AslRequest/FreeAslRequest with the program's tags, as MUI 3.8 makes them (logged on both: the same tags, the same pointers, asl's window). Contact Manager's drawer requester is ASL's again. Behaving as MUI 5 unchanged (Zune's panel) |

YAM 2.5's configuration window came after 15 s (MUI 3.8: 12 s; before: not
within 60 s, about 40 s in the suite's run). Each setup of it loads YAM's ~50
pictures, which fail on OS 3.1 ("Error messages"), and the old fitting set
YAM's windows up in more stages; how many setups that cost was not counted, so
the cause is inferred, not traced.

Library file 279,404 -> 279,844 bytes (+440); no memory per object (the list
and scrollgroup changes save one Scrollbar each where they apply).

Checked (FS-UAE, OS 3.1, 68000 build, GCC 16.2): `make check` passes after the
last commit and every commit builds; fullcheck: the 19 MUI 3.8 demos open and
close, no alert, memory as before (MUI-Demo 608 bytes, the others 152-264: the
positions file); the 18 demos and 7 of MUI-Demo's 8 sub-windows now open at MUI
3.8's sizes (before: 14 demos and 1 sub-window), MUI-Demo's Listviews 290x213 against
MUI 3.8's 292x199; the real-program suite against MUI
3.8: **30 of 36 programs as on MUI 3.8 (14 before)**, none that matched before
differs now; the MUI 4/5 group with the MUI 5 identity (`--groups mui5`) runs as
before: the same status, windows and memory for all five programs, `lean` and
`suitefix` run the same day; only the MUI_Request windows changed their layout
(YAM 2.10dev's "Confirmation Request" 388x88 -> 249x103, AmigaGPT's error
564x91, was 580x91: the requester layout is Zune's look for both identities, as
the frames are).

Still different, and why:
- TheBar demo2's control window and demo3 18 pixels wider; AmIRC's server
  window opens 256 high (MUI 3.8 236), AmTelnet 248 at y 4 (MUI 3.8 256 at
  0): window default sizes, not measured further yet.
- IBrowse 2.5.9: "Couldn't open initial window" (its main window does not
  fit even in the last stage on Zune); its later freeze is FS-UAE crashing.
- MultiRen: the suite marks MUI 3.8's run as "still running after quit";
  Zune's quits. Same windows and sizes.
- An empty group: MUI 3.8 gives it a negative height (minus the spacing)
  and caps its parent's maximum width; Zune gives it 0x0.
- A Cycle's internal structure (Zune: a page group of texts; MUI 3.8: one
  text) and a List's (an entry area inside the list group) differ; the
  outer sizes are the same.

## Each program gets the MUI it asked for (2026-11-06, phase 8)

Branch `identity`. Programs and classes choose their code paths by the
version muimaster.library's base reports (TheBar sizes its virtual bar
itself only below MUI 4; NList, TextEditor and BetterString branch on MUI 4
revisions), so one number for the whole machine was wrong for somebody:
19.35 kept MUI 4/5 programs out, 21.227 gave MUI 3 programs MUI 5's
behaviour. Now each program sees the MUI it asked for, with no setting to
change:

- **How exec helps.** OpenLibrary() checks lib_Version against the version
  asked for *before* it calls the library's Open, and returns whatever Open
  returns. So the base in the library list reports MUI 5 (21.227) and lets
  every program in, and Open hands out a base for the program: that base
  itself for MUI 5, or a copy reporting 19.35 (MUI 3.8) or 20.6000 (MUI 4).
  A copy is the jump table and the Library structure (342 bytes), made the
  first time it is needed and shared by every program of that identity; its
  vectors are the same functions, so a call costs nothing more. Close and
  Expunge through a copy work on the shared base.
- **Where the version is.** The RKRM's example library takes it in D0; exec
  40.10 (OS 3.1) and 47.8 (OS 3.2) call Open with the base in D0 and the
  version in D2 (measured: D2 was 0, 5, 19, 20, 21 as asked, through
  OldOpenLibrary too, and for a library ramlib had just loaded). Open takes
  D0 when it is not the base, else D2.
- **Automatic** (the default): asked for 0-19 MUI 3.8, 20 MUI 4, 21 or
  more MUI 5. Zune prefs, System page, "MUI version": **Behave as**
  Automatic / MUI 3.8 / MUI 4 / MUI 5 for all programs, and a list of
  programs with a MUI of their own (the program's file name: the Shell's
  command name or, from Workbench, the task name; no path, no case), which
  wins over the setting for all. Saved as ENV:Zune/BehaveAs (first line
  `Auto`, `MUI3.8`, `MUI4` or `MUI5`, then one `MUI5 YAM` line per
  program). Open runs under Forbid and cannot read it: the library reads it
  when it is loaded, and Zune prefs has the libraries in memory read it
  again when it saves or uses (their reserved vector, LVO -24, with 'ZnID'
  in D0), so the change holds for the next program started.
  `tests/lean/idtest reload` does the same after an edit by hand.
  ENV:Zune/Identity of the earlier "MUI 3.8 / MUI 5" setting is no longer
  read.
- **The program, not the machine.** The program's task is remembered while
  it has the library open (32 records of 12 bytes in the base: task, its
  stack, count, identity): its later opens keep its identity, its
  application object keeps it (MUI_GlobalInfo's private first field, high
  byte), and Zune's own MUI 3.8 / MUI 5 differences (List's horizontal
  scroller, MUI_AslRequest with asl.library's requester) follow the
  application, or the calling task where there is no object.
- **Classes.** External classes (.mcc) open muimaster.library in their init,
  which ramlib runs in its own task; Zune sets the identity of the program
  it is loading the class for around the load, and ramlib's opens get it
  (else the rules with the version the class asks for). A class library
  then serves every program with that identity while it is loaded. When
  nobody uses it any more and a program of another identity asks for it,
  Zune removes it from memory first (RemLibrary) so that it is loaded again
  for the new program: TheBar demo9 alone, Automatic, has MUI 3.8's layout
  (fits on 640 with the small font), and with a `MUI5 demo9` line MUI 5's
  (cut off at 640, topaz), one after the other.

Measured (FS-UAE A1200, OS 3.1, 68000 build, GCC 16.2):

| | lean | identity |
|---|---|---|
| library file | 279,844 | 282,084 (+2,240) |
| library + first program (idtest, in use) | 355,344 | 358,200 (+2,856: code, the records in the base, the MUI 3.8 copy) |
| each further program | 72,352 | 72,352 (+0) |
| the first MUI 4 program | - | +352 (its copy, once) |

`tests/lean/idtest.c` opens the library with a version and prints the base's
version, the list's and the identity Zune gave its application object:
0 and 19 -> 19.35 (identity MUI 3.8), 20 -> 20.6000 (MUI 4), 21 -> 21.227
(MUI 5) on OS 3.1 and 3.2.1; a second open of the same program with another
version keeps the first; three running at once (19, 21, 20) each keep their
own to the end. The settings for all and per program (also a line for a
program not running, names in other case) give what they say; Zune prefs'
list adds, changes and removes lines, saves, and the next program gets the
new setting.

What the suite's MUI 4/5 programs ask for (traced): YAM 2.10dev, IBrowse
3.0a, AmigaGPT 3.4 and MQTTExplorer 19 (IBrowse also 17), DizzyTorrent 20;
classes from ramlib 19 and 11. They are MUI 3.8 programs that use MUI 4/5
features when the version says so: Automatic gives them MUI 3.8 (they run
as they do on MUI 3.8); a line in the list gives one MUI 5.

Checked (FS-UAE, OS 3.1 unless said, 68000 build, GCC 16.2): `make check`
passes (its 68000 harness now models FindTask and opens with version 21);
fullcheck: the 19 MUI 3.8 demos and MUI-Demo's sub-windows exactly as on
`lean` (same windows, sizes and memory, run the same day); the real-program
suite, Automatic: **MUI 3.x group 30 of 36 as on MUI 3.8, all 36 the same
as `lean`'s run** (the same six differences, see above); MUI 4/5 group:
4 of 5 the same as with the MUI 5 setting before, AmigaGPT's window 2
pixels lower (590x253, was 255: it asks for 19 and now gets MUI 3.8).

Found on the way, also on `lean` (commit "Window: a remembered size is
kept within the window's own limits"): TheBar's demos all use application
base THEBAREXAMPLE and window ID MAIN, so demo1 started after demo9 opened
at demo9's remembered 627x220, above its maximum height, and drew nothing.
The remembered size is now clamped to the window's minimum and maximum.

Limits:
- A class loaded once serves all programs with the identity of the one it
  was loaded for while any program uses it (TheBar decides its MUI 4 mode
  at init; NList and TextEditor read their own base's version each time,
  and that base is the first program's). It is loaded again for another
  identity only once nobody uses it.
- Two programs loading classes at the same moment can swap the loading
  identity (no lock: a class whose init loads another class through ramlib
  would deadlock on one).
- A class a program opens itself with OpenLibrary (not through MUI) gets
  the rules with the version the class asks for, not the program's.
- SetFunction() on muimaster.library patches the listed base; programs
  holding a copy call the original functions.
- More than 32 programs with the library open at once: the others get the
  rules but no record (their objects fall back to the setting for all,
  else MUI 3.8).
- The version detection relies on exec 40/47 keeping the version in D2;
  another exec that passes neither D0 nor D2 would make every program
  MUI 3.8 under Automatic (the per-program list still works).

## The demo set pixel by pixel against MUI 3.8 (2026-11-06, phase 2)

`tests/lean/scripts/demopix.sh` shoots the 19 MUI 3.8 demos and MUI-Demo's
eight sub-windows on Zune (`a1200-31` with the library) and on MUI 3.8
(`a1200-31-mui38`), both with 8 MB fast RAM, default settings (no ENV:MUI,
no ENV:Zune), the same 640x256 4-colour Workbench and topaz 8; each shot
waits until two grabs in a row are the same. `demopix-compare.py` crops every
window, matches them by title, counts the differing pixels and writes
`$ZWORK/demopix/report.html` (both windows, a difference picture, the
counts) and `results.tsv`. One run of both sides takes about 3 minutes;
`--only zune` re-shoots one side, `SHOT...` some shots.

```
make -C workbench/libs/muimaster -f GNUmakefile core prefs plugins BUILD=/tmp/b LDFLAGS=-s
tests/lean/scripts/demopix.sh --lib /tmp/b/muimaster.library      # ports 1442 and 1443
```

**Result: 8 of 27 windows pixel-identical on `lean`, 19 now** (25 can be
compared: Layout places its buttons at random, InputHandler's text changes
every second). Every one of the 27 has MUI 3.8's size but MUI-Demo's
Listviews (2 pixels narrower). MUI 3.8 was measured black-box only:
screenshots, test programs reading public fields (`tests/lean/probe/`:
`propvis` the scrollbars of 232 lists, `objdump`/`zspy` object trees,
`tests/native/hdist.c` group distribution), its autodocs and examples.
All changes are for programs that get MUI 3.8 (identity, phase 8); MUI 3.9+
programs keep MUI 3.9's behaviour.

| commit | change (demopix windows it fixed) |
|---|---|
| 3ca0136c Text | a shortcut underline as long as the letter's advance (MUI 3.9: one pixel shorter). Popup |
| (dropped) Images | MUI 3.8's 10-row PopDrawer glyph was measured but not taken: copying artwork from screenshots is outside the clean-room rules; Zune keeps its own drawing. Popup |
| 226976dd List | whole rows only, the spare pixels shared above and below them (MUI 3.9: a cut-off last row); the scrollbar gets entries, visible and first in pixels (rows x row height, delta factor the row height): Intuition's prop gadget rounds those into other knobs than rows, `propvis` measured 232 lists, now all knobs as MUI 3.8's (137 differed). Groups, Notify, DragnDrop, Strings, ... |
| d11963cd Floattext | justifies to the whole entry area (MUI 3.9 leaves 4 pixels). Cycle |
| bd517f42, 49da8e37 Rectangle | with thick frames a VBar is two dark and two light columns and bars are at least 4 pixels wide (`objdump`: MUIO_VBar 1 and 2). Menus |
| 898869bf Text | a line with bold parts is centred as if wider by each part's overhang plus one (sizes unchanged). MUI-Demo's title |
| ae436362 Area | background patterns start at the window's corner, not the object's. Backfill |
| dbd7b283 Dirlist | sorted as MUIA_Dirlist_SortType/SortDirs/SortHighLow ask (drawers first, then by name: it was ExNext order, for every identity); behaving as MUI 3.8 no drawers with FilterDrawers and no accept pattern, sizes grouped with dots whatever the locale (6.468 on a United States locale). EnvBrowser |
| a6248bfb Volumelist | only devices whose handler runs (no CON:, PIPE:...), each kind sorted by name, a disk image for DF0:/DH0:, a chip for RAM:. Listviews |
| 66ef521b, ba21361a List | the title is the first row of the entry area, one empty pixel row below it, no separator (MUI 3.9's title buttons above the frame are not MUI 3.8's); a too long entry shows only its whole characters that end before the area's last column. Listviews (height), Strings |
| 23a0600c Register | spare pixels widen the last tabs, the right corner rounded in the fill pen, titles centred on the tab's own width, the active one a row lower, the page frame's sides doubled with thick frames. Pages |
| 01882b1b Text | a line starting in italics moves right by the italics' overhang left of its start (the flag in a padding byte). Virtual's "Hello User !" |

Measured (FS-UAE, OS 3.1, 4 MB fast, MUI-Demo cold, three runs each,
`lean` / now): first program 501,048 / 501,848 bytes (+800: the library
file is 900 bytes larger), each further 218,392-218,504 / 217,224 (1.2 KB
less: no title strip, whole rows), first window 1.44-1.48 / 1.42 s.
`make check`, fullcheck (MUI-Demo with 8 sub-windows and the 18 others:
no alert, the same "lost" figures as `lean`, which are its ENV: window
records) and the real-program suite pass: MUI 3.x 30 of 36 as on MUI 3.8,
the same six as before, every program as on `identity`'s run but one
AmTelnet reset at its second quit in one of two whole runs (three runs of
it alone after that were clean); MUI 4/5 group all five as before.

Still different (`$ZWORK/demopix/report.html`):

- **Balancing** (1,085 pixels) and **Subtask** (450): how a group shares
  out spare pixels. MUI 3.8 gives them to the first children where a
  child of fixed size or a Balance is in the group (Balancing's rows,
  Subtask's Start button 10/8/9 against Zune's 9/8/10), but to the last ones
  in `hdist`'s rows of free objects (99/99/101), and leaves a child with a
  maximum of 40 at 39 (`tests/lean/probe/hdist-mui38.txt`,
  `vdist-mui38.txt`: 393 + 253 measured rows; Zune's MUI 3.9 algorithm
  matches 375 and every MUI 3.9 row). No rule found that fits all rows: a
  search of about 2,000 variants of the MUI 3.9 algorithm (initial split,
  divisor, rounding, which end gets the rest) got to 18 rows off.
- **Virtual** (432): the window-border scrollers of the first page show
  other knobs (MUI 3.8: horizontal about 64 %, vertical 44 %, which is the
  page's view against its virtual size; Zune 48 % and 50 %). The same
  Scrollgroup alone (`tests/lean/probe/borderprop.c`) gives its two Props
  the same numbers on both (432/282, 240/99), so the difference is in how
  the demo's window updates its border gadgets. Not traced further.
- **Listviews** (6,227): the Volumelist has 2 more pixels of inner spacing
  on the left (`objdump`: add 6 against Dirlist's 4), which makes the window
  292 wide; the assign image differs.
- **Images** (2,334): MUI 3.8 scales the vector images to the space given
  (the "Scale Engine" column), and its HardDisk, Disk, Chip and Drawer
  images differ from Zune's in a few pixels each. Copying the images pixel
  by pixel from screenshots would be copying artwork: left for a decision.
- **Slidorama** (6,921): MUI 3.8's Knob and Levelmeter look different
  (a dial, readable numbers): the same artwork question.

## A smaller library (2026-11-06, phase 3)

Branch `smaller`. More rarely used code into the on-demand modules
(`LIBS:Zune/Z*.mui`, see "Modules" above), one switch made a table. In each
case the library keeps what layout and the common path need (sizes, the
decision whether a feature is on at all) and calls the module only when the
feature is used, so a program that does not use it never loads it.

| commit | what moved | library file | module |
|---|---|---|---|
| 73579d3b Frames | drawing a custom frame (`frame.config`'s tiled pictures, `frame_tile.c`) into Zpicture.mui, which loaded the frame's pictures anyway | -2,288 | Zpicture 9,568 -> 12,044 |
| f7fda3ce Window | MUI 3.8's extra title bar gadgets (Popup, Prefs, Snapshot, Iconify; off by default) and the Popup gadget's window menu (`wintitle.c`); the window keeps the decision and the clicks | -3,124 | Zwintitle 4,764 |
| 7e27ee4a Frames | the rounded frame types 5, 7, 8, A and B (`frame_round.c`), which no default setting uses; their sizes stay in the library's table, only the drawing comes from the module (looked up once and kept: modules leave only at expunge). muigfx.library's frame table, made at library init, now asks for sizes only (`zune_zframe_get_sizes()`): `make check` showed that init would otherwise have loaded the module | -4,336 | Zframes 6,848 |
| 28d173c8, 8ede6a9c, 908a5702 Images | tried and taken back: the vector images 10-23 (MUII_Drawer ... MUII_TapeDown) as a module. The suite's LoadSeg trace showed it loaded in ten of 36 programs: NListtree's default node images are TapePlay, TapeDown and Drawer. With those (and the drawer-based ones) back in the library it still loaded in CManager, RO and ViewCSV (which images was not traced; probably volume images in the file manager and the tape arrows as sort markers), so every vector image is in the library again, as on `lean` | 0 | - |
| 35debed5 Window | MUIM_Window_DragObject and a window's event handling during a drag (`dragwin.c`) into Zdragdrop.mui, which a drag loads anyway for its image | -1,244 | Zdragdrop 5,536 -> 6,928 |
| 6a978479 Menus | MUIM_Menustrip_Popup (`menupopup.c`) into Zmenu.mui, which draws the pop-up menu anyway | -836 | Zmenu 9,596 -> 10,616 |
| 02594a0c List | MUIM_List_Edit/EndEdit (MUI 5's in-place editing) and List's MUIM_DragReport, MUIM_DragDrop, MUIM_CreateDragImage (`listx.c`; List's instance data in `classes/list_private.h`); called only for an editable list, an open editor or a drag (setting MUIA_List_Format no longer goes near it). Also: class files compiled without LTO and their table made with `gcc-nm` under `LTO=1` | -1,828 | Zlist 3,824 |
| b4450875 Configdata | the 58-case switch that stored each string setting became a field offset in the defaults table; start-up stores each string straight through its entry | -964 | - |

Library file **283,196 -> 268,572 bytes** (-14.3 KB, -5.2%; code 265,388
-> 251,548 bytes); the class and module files 221,204 -> 241,528 bytes on
disk (47 files), loaded only when used. MUI 3.8's muimaster.library:
166,412.

MUI-Demo, cold, OS 3.1 + 4 MB fast (FS-UAE, 68000 build, GCC 16.2; `m.sh`,
two `measure.sh` runs and `libonly.sh` each, `lean` and `smaller` in the
same session; emulator figures):

| | library file | library alone | first program | each further | first window |
|---|---|---|---|---|---|
| lean | 283,196 | 295,616 | 501,784 | 217,224-217,288 | 1.46-1.52 s |
| smaller | 268,572 | 281,800 | 487,920-487,968 | 217,224-217,288 | 1.42 s |

The library is in memory once, so the 13.8 KB come off the library alone and
the first program; each further program costs the same as before. The first
window opens as fast. Deepest stack use (`tests/lean/scripts/stackrun.sh`, MUI-Demo
with its eight sub-windows, Layout, Virtual, Pages, Slidorama): 5,388 /
2,264 / 2,676 / 2,236 / 2,300 bytes on both, unchanged.

perfbench (68EC020, `bench.sh`, 15 repetitions, `lean` and `smaller` (at
8ede6a9c) side by side, twice): create+dispose +0.1 / +1.4%, open+close 0.0 / 0.0%, redraw
-0.0 / +0.1%, relayout +0.6 / +0.4%, list insert +1.5 / -0.1%, list active
+3.3 / +2.1%, notify -0.3 / -0.4%, text set -0.1 / -0.3%. The same
library on both instances (`lean` against `lean`) differs as much: open+close
+3.3%, list active +1.0%, list insert +0.7%; `lean`'s "list active" ranged
1,141,552-1,179,850 ticks over four instances, `smaller`'s 1,165,608-1,179,167.
No batch is slower beyond that spread; the moved code is not on these paths.

**Which modules a MUI 3 program loads** (`wasabi snoop` during the runs,
every LoadSeg of a `LIBS:Zune/` file, `tests/lean/scripts/snooplog.sh`):
fullcheck (MUI-Demo with its eight sub-windows and the 18 other MUI 3.8
demos) loads exactly the class files `lean` loads and no module. The whole
real-program suite (36 entries) loaded none of the new modules except the
image module of the first try (above), which is gone; the programs that had
loaded it (CManager, RO, ViewCSV, the NListtree demo, YAM 2.5) were run
again on the final build: no new module. The other module loads are
`lean`'s: Zrequest.mui where a program shows a MUI requester (MUIMine,
Archiver, IBrowse 2.5.9, NewsCoaster, RO) and Zpicture.mui for the
TextEditor demo's pictures.

Checked:

- `make check` after every commit.
- Screenshots: `tests/lean/smaller.c` (the five rounded frames, the fourteen
  images, the pop-up menu strip open, the window menu of the Popup title
  gadget with all four title gadgets on through `tools/lean/mkprefs.py`)
  pixel-identical to `lean` after each step and on the final build.
- fullcheck: MUI-Demo with eight sub-windows and the 18 other demos open and
  close, no alert, the same "lost" figures as `lean` in the same session
  (they are the programs' window position records in ENV:).
- Demo pixel comparison (`demopix.sh`): 18 of 27 windows identical, the same
  as `lean` (Popup differs by its PopDrawer glyph since 639730ba: Zune's own
  drawing); every count as `lean`'s.
- Real-program suite: MUI 3.x group 30 of 36 as on MUI 3.8 (the same six
  differ: TheBar demo2/demo3, IBrowse 2.5.9, AmIRC, AmTelnet, MultiRen) and
  36 of 36 as `lean`'s (`demopix`) run; MUI 4/5 group 5 of 5 as `lean`'s.
- The whole suite ran with the image module still in (28d173c8), demopix
  and perfbench on 8ede6a9c; on the final build (908a5702: every image in
  the library as on `lean`) make check, fullcheck, the stack, the
  `smaller.c` screenshots, the memory figures and the five programs that had
  loaded the image module were run again, all as above.

**LTO** (`LTO=1`, GCC 16.2, measured, not switched on): -14.0 KB more (on
02594a0c: 265,744 -> 251,408 bytes; the screenshots of `tests/lean/smaller.c`
identical), but stack use grows: MUI-Demo's deepest stack 5,388 -> 5,992
bytes, Virtual 2,676 -> 3,312 (functions called once inlined across files
into the dispatchers). With `-fno-inline-functions-called-once` the gain is
11.2 KB, but that build did not load its modules and MUI-Demo did not
start: not looked into further. And `make check` cannot run on an LTO
build: the checks read the library's hunk symbols, which the m68k linker
writes wrong under LTO (the harness ends in a memory error). Class files must be compiled without
LTO (else they pull in libnix's stdio and do not link) and their function
table needs `m68k-amigaos-gcc-nm` (the GNUmakefile does both now).
`-ffunction-sections -fdata-sections` with `--gc-sections` makes the hunk
library 7.5 KB larger: no use.

**What is left toward 166 KB** (per-function sizes with `nm -n` on the
symbol build): the rest is the core classes' own code that every program
runs: List 29 KB, Window 27 KB, Group 21 KB, String 20 KB, Area 16 KB,
Application 15 KB, text engine 10 KB, Configdata 8 KB, vector images 8
KB, frames 9 KB. Most of the gap is in what every program runs, not in
rarely used features.
Still movable, small: String's Textinput-compatible attributes and 64-bit
integers (MUI 4/5), Configdata's setters (prefs programs only, ~0.7 KB),
the private Applist object (LVO -264, AmIRC only, ~0.5 KB), the Aboutmui,
Settings, Settingsgroup and Mccprefs classes as one file (~1.3 KB). Beyond
that it means rewriting the big classes smaller (roadmap "Later").

## Memory per program (2026-11-06, phase 4)

Branch `memory`. Target: first program <= 460 KB, each further <= 210 KB
(MUI-Demo; MUI 3.8 460 / 211-219 KB). Measured first where the memory
goes, then made the instance data of the classes every program uses as
small as MUI 3.8's or smaller, and moved what only an open window needs
into blocks made when it opens. All figures FS-UAE (A1200, OS 3.1 + 4 MB
fast, 68000 build, GCC 16.2), emulator figures, bytes; `m.sh` (two
`measure.sh` runs and `libonly.sh`), `lean` and MUI 3.8 in the same session:

| | library file | library alone | first program | each further | first window |
|---|---|---|---|---|---|
| lean (= smaller) | 268,572 | 281,848 | 487,968-488,016 | 217,224-217,288 | 1.42-1.44 s |
| memory | 269,188 | 282,368 | 479,064 | 207,704-207,768 | 1.42-1.44 s |
| MUI 3.8 | 166,412 | 238,480 | 460,296 | 210,696 / 218,824 (it alternates) | 1.46 s |

Each further program: -9.5 KB, below the target and below MUI 3.8. The
first program: -8.9 KB, the target is not reached; what is left is library
code (below). The library itself grew by 616 bytes of code (library alone
+520 bytes). ("KB" here is 1,000 bytes, as in the targets; the table at the
top of this file is in KiB.)

**Where the memory is** (`tally.sh`: the second MUI-Demo's own blocks,
AllocMem/FreeMem patched; `libtally.sh`: every task's blocks while the
library is opened or the first program runs; `tools/lean/mtreport2.py ...
cats|tasks`):

| second MUI-Demo's own task | lean | memory | MUI 3.8 |
|---|---|---|---|
| total | 144.6 KB / 949 blocks | 135.5 KB / 950 | 138.8 KB / 128 |
| objects (instance data) | 84.9 KB / 334 | 76.2 KB / 334 | in ten 8 KB pool puddles (80 KB with notifications and strings) |
| lists (pools, entries, columns) | 19.3 KB | 19.3 KB | 13 x 2 KB pools |
| smart refresh backing store | 15.2 KB | 15.2 KB | 14.9 KB |
| notifications | 8.4 KB | 8.4 KB | (pool) |
| text engine, strings, Text contents | 8.8 KB | 8.3 KB | (pool) |

- **The library alone is code.** `memtally lib`: ramlib holds 256.6 KB for
  Zune (the hunks, 251.5 KB of code, and the 1,282-byte base), 204.5 KB for
  MUI 3.8 (muimaster, muigfx, muilowlevel) plus 9 KB MUI 3.8 allocates in
  the opener's Open; the other ~25 KB of `libonly.sh`'s figure is HoldLib's
  process, the same on both. Zune's first program on top of the library
  costs *less* than MUI 3.8's (206 against 222 KB: `memtally runall`,
  the program's task 149 KB + 8.7 KB loaded by ramlib, against 136.6 KB
  + 37 KB of class files and muirexx), so the 19 KB still missing for
  460 KB are the 43 KB of extra code ("A smaller library" above).
- **AllocMem rounds to 8 bytes**: an object is 12 (BOOPSI) + 28 (Notify) +
  Area + its class's data, so data sizes count in steps of 8. MUI-Demo
  holds 334 objects: 72 Text, 72 Image, 58 groups, 45 Rectangle, 20 Prop,
  18 lists, 13 scrollbars, 13 listviews, 9 windows (one open).

| commit | change | object size (bytes) |
|---|---|---|
| 072a96c4 Area, Group | Area 114 -> 108 (MUI 3.8's size): MUIA_FixWidth/FixHeight are read from mad_FixWidth/mad_FixHeight (offsets 64/68, the same values), MUIA_MaxWidth/MaxHeight go into the rare block (16 -> 20 bytes, the same 24-byte AllocVec), the event node's priority uses the unused mad_FrameOBSOLETE byte. Group 96 -> 68: counts, spacings and same-size values WORDs, no pointer to its own child list | every object -8; a group 256 -> 216 |
| ca772188 Window | the render info's clip region stack, buffer rastport and custom frames (244 bytes, `mri_Tail`) made at the window's first setup and kept until dispose; Window data 730 -> 490 | a window 770 -> 536 (530 in the tally) |
| b732b9e9 Image, Text | flags, states and modes as bytes: Image 36 -> 28, Text 50 -> 38 | image 192 -> 176, Text 208 -> 192 |
| b99e43d4 Text | `zune_text_compact()`: a Text's parse, kept from setup to cleanup, is copied into a block of the size it used (the 192-byte arena's rest given back; links and inline strings moved, chunks from outside the arena relinked); `tests/check_ztext_compact.py` | -72..112 per shown title with a preparse or shortcut |
| e7dfd36c List | the BOOLs, enums and small modes as bytes at the end: List 420 -> 392 (MUI 3.8: 390) | a list 640 -> 608 |
| d6f43f8e | stack use as before (below) | |

The objects the checks fake at fixed offsets (Area's DisableCount, Group's
update/virt/child list, window data, render info) follow the new layouts;
the offsets were taken with `offsetof` from the real sources.

**Stack**: the smaller data first cost stack: GCC kept one more register
in Group's dispatcher, AskMinMax and FindAreaObject (+4 bytes per nesting
level), Image's AskMinMax, Text's and Window's dispose, List's OM_SET
(`-fstack-usage` on `lean` and this branch, compared per function).
d6f43f8e keeps Group's flags a ULONG (the two forwarding switches are bits:
still 68 bytes), takes the child list through a pointer the compiler cannot
see through, puts Image's and Text's byte fields first, keeps List's update
mode an int: every function's frame is now that of `lean` or smaller.
`stackrun.sh`: MUI-Demo with its eight sub-windows 5,388 -> 5,384 bytes,
Layout 2,264, Pages 2,236, Slidorama 2,300 as before; Virtual 2,672 four
times in a row (`lean`: 2,676, 2,732, 2,408, 2,740 and 2,676 in five runs:
Virtual's figure moves by about 60 bytes between runs of the same library,
a task switch at the deepest point puts the saved registers on the stack).

**Speed** (perfbench, 68EC020, `bench.sh`, 15 repetitions, `lean` and
`memory` on two instances, then swapped): create+dispose +0.6 / -1.1%,
open+close +4.9 / -0.0%, redraw -0.2 / -0.1%, relayout -0.5 / -0.4%, list
insert +0.2 / +0.8%, list active +0.7 / +3.2%, notify 0.0 / 0.0%, text set
-0.0 / -0.1%. The same library on two instances differs as much
(`memory` open+close 269,550 and 282,992 ticks; `lean` list active 823,763
and 801,332): no batch is slower beyond that spread.

Checked:

- `make check` after every commit (36 checks; the new
  `check_ztext_compact.py`).
- fullcheck: MUI-Demo with eight sub-windows and the 18 other demos open and
  close, no alert; "lost" 176-264 bytes per demo, 608 for MUI-Demo, as
  `lean` in the same session (the programs' window position files).
- Demo pixel comparison (`demopix.sh --only zune` against `smaller`'s MUI
  3.8 shots): 18 of 27 identical, every count as `smaller`'s except Layout
  (3,221 -> 3,168 differing pixels: the demo places its buttons at random).
- Real-program suite (`suite.sh`): MUI 3.x group 30 of 36 as on MUI 3.8
  (the same six differ), 36 of 36 as `smaller`'s run; MUI 4/5 group 5 of 5
  as `smaller`'s. Memory in use per real program (the suite's new column,
  [real-programs.md](../../tests/lean/real-programs.md)): 34 MUI 3.x programs
  212,584 bytes less together (0.3-26.7 KB each: RO -26.7, MultiRen -25.8,
  WookieChat -24.3, AmigaAMP-Prefs -20.1, NewsCoaster -19.1 KB), YAM 2.10
  -92.6 KB, AmigaGPT -17.4, DizzyTorrent -7.2. Most still hold 10-110 KB
  more than on MUI 3.8 (RO 410 against 299 KB): not traced yet.

**Left open**: the first program (19 KB of library code, phase 3/"Later");
the list pools (13 x 1 KB in a further MUI-Demo, MUI 3.8 13 x 2 KB), the
notifications (8.4 KB, 32 bytes each) and the smart refresh backing store
(as MUI 3.8) were not touched; Window's pens, title gadgets and snapshot
state (~60 bytes) could join `mri_Tail`; `Register`'s titles could use
`zune_text_compact()` too (class file, not measured).

## Real programs' memory against MUI 3.8 (2026-11-06, phase 4 follow-up)

Branch `realmem`. The suite's "memory in use" column (the second run,
windows open) showed Zune 10-110 KB above MUI 3.8 in most real MUI 3
programs (RO 410 against 299 KB, AmFTP 406 against 308, AmigaAMP-Prefs 326
against 213) while MUI-Demo's objects were below MUI 3.8's. Traced
black-box with memtally on both sides: every task's AllocMem/FreeMem with
its callers, LoadSeg'd code told apart from data; MUI 3.8 only from outside
(which library asked, which task, block sizes, what stays after a quit).
Emulator figures (FS-UAE, A1200, OS 3.1, 8 MB fast, 68000 build, GCC 16.2),
bytes.

**Most of the gap was the measurement.** MUI 3.8 keeps its object pools'
8 KB puddles when a program quits, and the suite's second run fills them
again without asking exec for much. `memtally lib muimaster.library`
started before the first run and dumped after the program quit
(`suite.sh --tally`, `TALLY_RUN=keep`): MUI 3.8 still holds 112 KB of
puddles for AmFTP (in the program's task and in a subtask's), 80 KB for RO,
56 KB for AmigaAMP-Prefs, until a flush; Zune holds none. Everything that
stays after the quit apart from code: MUI 3.8 154 / 118 / 93 KB, Zune 33 /
28 / 28 KB (fonts on both). So the second-run column compares all that Zune
needs with what MUI 3.8 adds to memory it kept from the first run.

The fair comparison is the first run from a freshly booted machine. The
suite now records it too (results.tsv's seventh column, printed by
`suite-compare.py`), and the tally splits it into code (all that ramlib
loaded and every LoadSeg'd hunk: libraries, class files, the program) and
data (all the rest), `tools/lean/mtlib.py`, `mtsizes.py`; every task:

| first run, windows open | lean data | realmem data | MUI 3.8 data | lean code | realmem code | MUI 3.8 code |
|---|---:|---:|---:|---:|---:|---:|
| AmFTP 1.91 | 297,334 | 272,346 | 326,309 | 520,094 | 520,646 | 474,035 |
| RO 1.27 | 271,800 | 238,060 | 230,689 | 450,178 | 450,730 | 395,229 |
| AmigaAMP-Prefs 1.25a | 281,989 | 246,189 | 224,598 | 397,978 | 398,530 | 338,371 |

The code is larger on Zune: its library is 269 KB against MUI 3.8's 166
KB, and Zune's class files are LoadSeg'd in the program's task (counted
here as code: RO 22 KB of them), MUI 3.8's by ramlib.

**What Zune really spent more**, per caller (memtally `runall`, Zune's
function names from a build with symbols), and what was done:

| commit | what | before |
|---|---|---|
| b89663cb Area | an object's live block (event, IDCMP, control-char and timer nodes, 120 bytes) was made at every object's first setup; labels, images, groups and spacers never get an event. Now only for objects with an input mode, a context menu, a shortcut or in the cycle chain, or on MUI_RequestIDCMP / MUIA_Timer; a shortcut, cycle chain or context menu set while shown makes it then. MUIA_Disabled on a shown object without one skips the simulated button release (the first build wrote through NULL there: YAM 2.5's configuration window reset the machine; `tests/check_area_live.py`) | RO 230 blocks, 27.6 KB; AmigaAMP-Prefs 34.8 KB; AmFTP 23.9 KB |
| 8fee6ff2 Image specs | MUII_BACKGROUND, SHADOW, SHINE and FILL (the settings' "0:128" and friends, the background most objects get) allocated a 50-byte spec per object at every setup. Four shared constants now, as the pattern specs already were; a MUI pen is read from the render info it is drawn with (`zune_penspec_pen`), so one spec serves every window and screen | RO 360 blocks, 19.4 KB; AmFTP 6.5 KB; AmigaAMP-Prefs 5.9 KB |
| 5106711a Area | the MUI_RequestIDCMP node (old-style classes) and a pressed button's repeat timer moved out of the live block into one made at first use: 80 bytes per button, string or cycle gadget | RO 177 x 120 bytes |
| 898e030d | stack as on `lean` (below) | |

Library file 269,188 -> 269,572 bytes (+384 bytes of code). MUI-Demo
(`m.sh`, OS 3.1 + 4 MB fast): first program 479,064 -> 476,368, each
further 207,704-207,768 -> 204,456-204,504, library alone 282,392 ->
282,920, first window 1.42-1.44 s as before.

Real-program suite, memory in use (`suite-compare.py`, `lean` and MUI 3.8
the same session; [real-programs.md](../../tests/lean/real-programs.md) has
every program): 32 MUI 3.x programs 284,744 bytes less together in the
second run (RO -33.8 KB, AmigaAMP-Prefs -35.8, AmFTP -24.9, Archiver -18.7,
WookieChat -16.7, YAM 2.5 -15.0), the first run from boot 274,672 bytes
less; MUI 4/5 group: YAM 2.10 -222.8 KB, DizzyTorrent -11.7, AmigaGPT
-11.1. InstallerNG, AmiTradeCenter and Annotate hold the same as before
(as on every branch; not looked into).

**Stack**: `-fstack-usage` of every changed file against `lean`, per
function: the first build had `setup_control_char` 4 -> 20 bytes (it made
the live block itself; OM_SET makes it now) and `zune_imspec_free` 8 -> 12
(two ranges for the shared specs; one array now), and a guard inside
`handle_release` cost 4 bytes (it is in OM_SET's call now): every
function's frame is `lean`'s. `stackrun.sh` twice: MUI-Demo with its eight
sub-windows 5,384, Layout 2,264, Virtual 2,672, Pages 2,236, Slidorama
2,300 bytes, as `lean` (whose Virtual gave 2,672 and 2,736: its known
noise).

**Speed** (perfbench, 68EC020, `bench.sh`, 15 repetitions, this branch and
`lean` on two instances at once, then swapped): create+dispose -1.1 /
-2.1%, open+close -3.5 / -3.5%, redraw 0.0 / -0.0%, relayout +0.4 / +0.2%,
list insert +1.2 / -0.3%, list active +1.2 / -0.4%, notify +0.2 / +0.1%,
text set -0.0 / -0.1%. Opening a window is faster (fewer allocations); no
batch is slower beyond the known spread.

Checked:

- `make check` after every commit; the new `check_area_live.py` fails
  on the first build's bug and passes now.
- fullcheck: MUI-Demo with eight sub-windows and the 18 other demos open and
  close, no alert, the same "lost" figures as `lean` in the same session;
  all 26 screenshots pixel-identical to `lean`'s.
- Demo pixel comparison (`demopix.sh --only zune` against `memory`'s MUI 3.8
  shots): 18 of 27 identical, every count as before except Layout (its
  random buttons).
- Real-program suite: MUI 3.x group 35 of 36 as `lean`'s run of the same
  session, 30 of 36 windows as on MUI 3.8 (the same six differ); the one
  other difference is ViewCSV's second quit, which reset the machine on
  `lean` and on MUI 3.8 and not here (FS-UAE, as AmTelnet's). MUI 4/5
  group 5 of 5 as `lean`'s. YAM 2.5 was run by hand too (configuration
  window open for a minute).

**Left open**: each shown Text keeps its parse (~110 bytes per label:
AmigaAMP-Prefs 131 of them, 15 KB; RO's 104 empty texts 56 bytes each):
dropping it means parsing at every draw (slower) or a parse buffer on the
stack (deeper), so it stays. Notifications (24 bytes + 4 per parameter),
Text contents copies and object data are about MUI 3.8's. What is left
against MUI 3.8 in data: RO +7 KB, AmigaAMP-Prefs +22 KB (AmFTP -54 KB).
The code is the larger difference now (library and class files).

## The installer, image classes and the External list (2026-11-06)

Branch `installer`. Our partner: "Make a good installer that handles
things".

| commit | change |
|---|---|
| 7e6c547a Version | `$VER` of the libraries and class files: "muimaster.library 21.227 (DD.MM.YYYY) Zune68k lean", the date of the build (`$(BUILD)/build-date.h`, written only when it changes; `ZUNE68_DATE=DD.MM.YYYY` for a fixed one). Was "Zune68 Beta 2" and 02.11.2026 by hand. lib_Version and the identity logic unchanged; "Zune68" stays in the id string for what looks for it. The archive is `Zune68k-lean-YYYY.MM.DD-amigaos3-m68k.lha` |
| fdd845c1 Installer | `dist/Install` rewritten, `ZuneSwitch` (`workbench/tools/zuneswitch`), SYS:Zune68k with Use real MUI / Use Zune / Uninstall, a readme for the lean edition, `tests/check_installer.py` in `make check` |
| d0e433d8 Image specs | "3:<name>.image" (external BOOPSI image classes) drawn |
| d99e67f7 Imageadjust | the External page's previews made only for the rows on screen |
| ac600950 Zune prefs | a real MUI's own classes (MUI:Libs/mui) not opened |
| f4bf2d84 check_resources | the "3:" spec's class library answered "not found" in the host harness |
| 6be56048 ZuneSwitch | the Zune68k drawer also removed after an uninstall from a Shell |

### What the installer does

- **Looks first, changes nothing of MUI.** It reports a MUI (MUI: or
  SYS:MUI, its muimaster.library version: 19 MUI 3, 20 MUI 4, 21 MUI 5),
  MUI's block in S:User-Startup (`Search ";BEGIN MUI"`), muimaster.library
  in every LIBS: directory (Zune or not, `ZuneSwitch ISZUNE` looks for
  "Zune68" in the file; the directories from `ZuneSwitch LIBSDIR n`,
  Installer cannot walk a multi-assign), an earlier Zune, ENVARC:Zune and
  ENVARC:MUI. Zune goes into the first LIBS: directory (`expandpath
  "LIBS:"`), which ramlib searches before MUI:Libs; nothing is written to
  the startup files (Zune needs no assign).
- **A record of everything.** SYS:Zune68k/Data/Manifest: `NEW path`,
  `REPLACED path` + `BACKUP name` (the old file in Data/Backup), `DIR
  path`, paths as SYS:... when on the boot volume. Installer 43's strings
  end at 10,000 characters (measured: `cat` fails above), so the lines
  are appended in parts (`textfile` with `include` of the file so far).
- **MCCs.** A bundled class is installed into <first LIBS:>/MUI unless the
  same or a newer one is in a LIBS:MUI drawer (the first, or another LIBS:
  directory, up to three); an older one there goes to the backup. MUI's
  own drawer (MUI:Libs/mui) does not count: its classes are that MUI's
  (MUI 5's Aboutbox, Busy, Calltips, Clock, Lamp, Listtree, Rawimage,
  Textinput are all newer than Zune's and written for MUI 5's private
  interface); Zune uses its own and "Use real MUI" gives MUI its own back.
  The last page lists what happened to each class.
- **Update and remove.** Install again: "Install this version over it"
  (the earlier installation is removed first with `ZuneSwitch UNINSTALL
  QUIET`, so the backups come back and the new installation starts from
  the user's own files; settings kept) or "Remove Zune68k" (asks whether
  to keep ENVARC:Zune).
- **SYS:Zune68k**: Use real MUI, Use Zune (only when a real MUI was found),
  Uninstall, ReadMe; project icons whose default tool is
  SYS:Zune68k/Data/ZuneSwitch with `ACTION=MUI|ZUNE|UNINSTALL`. From a
  Shell: `ZuneSwitch MUI|ZUNE|UNINSTALL [SETTINGS] [QUIET] [HOME=dir]`,
  `STATUS`. Use real MUI moves every recorded file in the LIBS: directory
  (both libraries, LIBS:Zune's files, the classes) to Data/Parked and the
  user's replaced files back, after checking that a real MUI will then be
  found; a failed move undoes the ones before it. Uninstall deletes Zune's
  files, puts back the backups, removes the drawers it made when empty;
  from Workbench the drawer it runs from is deleted by a script started
  after it (`T:Zune68k-remove`, tries for two minutes: a Workbench window
  of the drawer locks it).
- **Installer versions.** 43 or newer (OS 3.1: 43.3, the one MUI 3.8's
  archive carries; OS 3.2.1: 47.19). The Installer on the OS 3.1 Install
  disk reports @installer-version 0: the script stops with a message
  naming Installer 43.3. Novice installs everything without questions,
  Intermediate/Expert ask about the translations, pretend mode changes
  nothing (checked: the drive identical to before).

### Tested (FS-UAE, the three test machines, every step a reboot between)

Screenshots in `/tmp/installer/shots/` (not kept in git).

| machine | install | program gets | Use real MUI | Use Zune | update | remove | after removal |
|---|---|---|---|---|---|---|---|
| a1200-31 (3.1, no MUI; Installer 43.3 put in C:) | Intermediate, 250 files | Zune (MUI-Demo) | not offered | - | - | Uninstall icon, "Remove all": nothing of Zune left (drive compared with the clean machine) | - |
| a1200-31-mui38 (MUI 3.8) | Intermediate, 254 files; Busy and Listtree: MUI 3.8's stay for MUI 3.8 | Zune | icon: MUI 3.8 19.35 after reboot | Shell: Zune after reboot | Intermediate, settings kept | Install again, "Remove Zune68k", settings removed | drive identical to the clean machine; MUI-Demo on MUI 3.8 |
| a1200-32-mui5 (3.2.1, MUI 5, Installer 47.19) | Expert pretend (nothing changed), Novice 253 files; a user Busy.mcc 16.36 replaced (backup), a user Listtree.mcc 21.40 kept | Zune | Shell: MUI 5 after reboot, the user's Busy 16.36 back | Shell: Zune | Expert, translations skipped | Uninstall icon, both "keep settings" and "Remove all" | user's Busy/Listtree byte-identical to before, MUI-Demo on MUI 5 |

Found on the way:
- **Zune prefs reset the MUI 5 machine** 3 s after its start: it opens
  every .mcc/.mcp of every LIBS: directory, MUI 5's own Listtree.mcp,
  LayGroup.mcp, Hyperlink.mcp... among them (lean before this branch too).
  Fixed in Zune prefs (ac600950). It still needs 84 s to open its window on
  the emulated 68020: 60 s without a DOS call after the settings are read
  (`wasabi snoop`), presumably the window fitting stages of its large
  window on a 640x256 screen; not looked into further.
- The "user's newer class is kept" rule also keeps a newer copy of a real
  MUI's own class that a user put in SYS:Libs/MUI (the test's MUI 5
  Listtree.mcc 21.40): Zune then loads it. Not changed: the rule is the
  user's choice; the installer says so on its last page.
- Two packs with an image class of the same file name: exec finds an
  already loaded library by its name, so the first one loaded serves both.

Regression checks on the final build (FS-UAE, 2026-11-06/07): `make check`
passes (with `check_installer.py`); fullcheck: 19 demos + MUI-Demo's 8
sub-windows open and close, no alert, lost bytes as before (MUI-Demo 608,
the others 176-264: the positions files); demopix 18 of 27 windows
pixel-identical (as before); suite: MUI 3 group 29 of 36 as on MUI 3.8 -
the six known differences plus SimpleMail "nowindow", which the lean
baseline run (suitefix `after`) also had (SimpleMail takes wasabid down);
MUI 4/5 group: all five run, 4 of 5 the same as the lean baseline, AmigaGPT
2 pixels lower (as noted for `identity`).

### External BOOPSI image classes ("3:<name>.image")

`imspec_boopsi.c`, in the picture module: OpenLibrary of the file by path
(the image drawers LIBS:Zune/Images/ and MUI:Images/, or the name as it is
with a volume; Lock first, pr_WindowPtr -1), NewObject of the public class
(file name, then without ".image", then a ClassLibrary's cl_Class),
DrawImageState at the object's size (IA_Width/IA_Height set when the area
differs), IDS_SELECTED for a selected image, DisposeObject + CloseLibrary at
cleanup. The sizes: an image class written for MUI answers method
0x80427157 (longs at 12..32: minimum, maximum, default width/height; at 36
the MUI_RenderInfo it keeps for MUI's pens), seen in the dispatcher of the
kmel images that ship with MUI 5 (third-party code, not MUI's); MUI 5 shows
a fixed image at its minimum. Other classes get their object's size (an
imageclass object without a size is 80x40, no IM_DOMAINFRAME in the kmel
classes, `tests/lean/probe/imgclass.c`).

Measured (FS-UAE, OS 3.2.1, `tests/lean/boopsiimg.c`, black box against
MUI 5): the 19 kmel images and Misc/mui-stdcycle.image have the same sizes
as on MUI 5 (4x4 arrows, 8x8 for the "2" ones, 12x8 checkmark, 16x8 cycle,
3x3/7x7 knobs, 6x8 popfile/popup, 15x9 stdcycle), a 40x24 stretched image
too; the pictures look the same. Library +416 bytes (the setup, cleanup,
size and draw cases and two module calls), Zpicture.mui +1,264 bytes.

### The External page

Every listed file got its preview when it was listed: an Image object whose
picture datatypes loaded at the list's setup, about 0.6 s each on the
emulated 14 MHz 68020. Now files are listed by name and the previews of the
rows on screen are made afterwards (the list notifies MUIA_List_Visible and
MUIA_List_First; opening a drawer asks too), one per pushed method so that
rows that grow make fewer of them visible and input is handled in between;
the header check that keeps logos out moved with them. Closing a drawer
frees its previews as before.

| FS-UAE A1200 3.2.1, MUI 5's MUI:Images (emulator) | before | after |
|---|---|---|
| External page shown (MUI:Images: 10 drawers, 13 pictures) | 8.2 s | 0.6-1.2 s |
| default_OS3 opened (39 pictures) | 11.2 s | 4.2 s (the 4-5 on screen) |
| kmel opened (19 image classes) | - | 0.6 s |

Imageadjust.mui +492 bytes.

## Zune prefs opens in 6 s instead of 84 (2026-11-07)

Branch `prefsfast`. SYS:Prefs/Zune took 84 s to open its window in
FS-UAE (A1200 68EC020, OS 3.1, 8 MB fast RAM, 640x256 Workbench, 68000
build), about 60 s of it CPU with no disk access. Measured with time
stamps in the program (`-DPREFS_TIMING` builds print them) and a small
profiler (`tests/lean/pcsample.c`: every tick it keeps the saved PC and
48 stack longs of the named task while that task is ready; it finds
the hunks of the program, muimaster/zunemaster, their class files and of
class libraries; `tests/lean/scripts/pcsample.py` maps it to functions
through the unstripped files, `CHAINS=label` prints callers).

Where the time went and what was done (emulator times, cold boot):

| | before | after |
|---|---|---|
| NBitmap dithering the five MCP icons (256-colour nearest search, three software multiplies per colour per pixel, 256 ObtainBestPen per setup) | 55 s | 0.2 s |
| Pixmap bzip2 of the twelve page icons (small mode: a search per output byte) | 2.1 s | 0.8 s |
| making all 17 pages before the window | 2.4 s | 0.2 s (the shown page only) |
| opening every .mcc in LIBS:MUI to ask for a settings class | 1.9 s | 0.9 s (.mcp files only) |
| five window setups (the window does not fit 640x256 at stage 0, see below), list images dithered again each time | 5 x 1.3 s | 0.6 s + 4 x 0.3 s |
| **window open, from the command** | **84 s** | **5.7 s** |

- **NBitmap.mcc** (`DitherImage.c`, `NBitmap.c`): the default colour map
  is a 3-3-2 cube, so the nearest colour is the nearest level per channel
  (checked equal to the full search, ties included, for every colour on
  the PC); other maps search with 16-bit multiplies, and a pixel the same
  colour as the one before is not searched again. Pens are obtained only
  for the colours the images use (all 256 before, on every setup).
- **Pixmap** (`classes/pixmap.c`, the Rawimage/Pixmap.mui file): bzip2 fast
  mode for images up to 64 KB (a few KB of tables for an icon); a small
  dithered image (<= 4096 pixels) is kept over a cleanup with its pens
  given back, and the next setup uses it if the same colours get the same
  pens again (the pixels are pen numbers: the picture is exactly what
  dithering again gives; other pens: dithered again). No division per
  pixel for a solid image or a repeated colour. `tests/check_pixmap_render.py`
  checks both the reuse and the dither-again path.
- **Zune prefs** (`zune.c`): pages are made when first shown, with the
  settings last given to all pages; Save, Use, Test, Save As and Open make
  the missing pages first (the first Test or Save in a run takes up to
  2 s more for that). Saved settings are the same as before (compared
  byte by byte from the same start: equal except one item, below).
  `find_mcps` opens only `#?.mcp` in LIBS:MUI (MUI's `.mcc` files give no
  settings class; the AROS drawers Zune/ and Classes/Zune/ still take both).

Found then, fixed on branch `rtgfix` (next section): the window did not
fit 640x256 at its normal fonts (670 x 257); one 6-byte item of
global.prefs held different bytes on every Save; Pixmap.mui carried
bzip2's compressor.

## RTG: MUI_AslRequest, Zune prefs on 640x256 (2026-11-07)

Branch `rtgfix`.

**The asl requester that returned at once on the A1200** (roadmap "Bugs
found from outside" 1). Reproduced in FS-UAE on an RTG screen: A1200 OS
3.2.1 (the `a1200-32-mui5` machine, its MUI 5 classes moved aside) with a
68030, `graphics_card = uaegfx-z3`, Picasso96 3.6.3 (iComp, from the
user's Picasso96.adf; FS-UAE has its own uaegfx.card, the monitor needs
the tool type `BOARDTYPE=uaegfx`) and a 1280x1024 24-bit Workbench.
`asltest 19` and `asltest 21` returned FALSE at once there, as on the
A1200; `asltest 19 raw` (asl.library called directly) too. The test
passed `_window(win)` as `ASLFR_Window`, as PFS3Doctor 0.1b1 did. With
`MUIA_Window_Window` all of them opened the requester: the window pointer
was the fault.

`_window()`, `_screen()`, `_rp()`, ... are the SDK's macros for area
objects: they read the long after the notify data as a `MUI_RenderInfo`
pointer. On a Window object Zune had its own render info there, so its
first field (the window object) was read as the render info and
`_window(win)` gave a pointer into the notify data (`mnd_NotifyList`'s
area), which asl.library took for a window: FALSE on the RTG screen, a
jump into garbage (the A1200's illegal-instruction reset) when called
directly. On planar 640x256 machines the garbage happened to pass.
MUI 3.8, measured black-box (`winprobe`: the object's longs, FS-UAE
a1200-31-mui38): the long points to a block whose public fields are all
0, so `_window(win)` and `_screen(win)` are NULL, open or closed, and the
requester opens on the default public screen. Zune's Window now starts
with a pointer to such a block (`window_area_renderinfo`, 32 bytes of
zeros; 4 bytes more per Window object). Checked: all four `asltest`
variants with `macro` open the requester on the RTG screen.

**Zune prefs fits 640x256 at the default fonts.** Every page's minimum
size, printed by a `-DPREFS_SIZES` build (stage 0, RTG 1280x1024; the
page list and the buttons add 178 x 40, the window 16 x 21 more, so a
page may be 446 x 195 on 640x256):

| page | before | after |
|---|---|---|
| System | 472 x 217 | 407 x 162 |
| Windows, Groups, Listviews, Navigation | 470-486 wide | the same |
| TextEditor, NListviews, NListtree (MCPs) | 502 x 230, 536 x 272, 442 x 221 | the same |
| the other 8 | fit | fit |

The System page's groups are two columns of their own (a 2-column grid
gave each row the height of its larger group) and the Public screen
group's rows no longer share one label column ("Pop to front:" widened
the name's label column). On the OS 3.1 640x256 Workbench the window now
opens at once in the normal fonts: one list setup instead of five, window
open 4.7 -> 3.3 s (FS-UAE, `PREFS_TIMING`, same instance and boot state).
The seven pages that are still larger than 640x256 at stage 0 were cut
off at the screen's edge when shown (seen on the Listviews page): Zune
prefs now opens its window again when a page does not fit, after the
input event, and the window's fitting takes it from the application's
stage down as at the first opening (Listviews then shows in helvetica;
the stage stays for the rest of the run).

**global.prefs changed on every Save**: TextEditor.mcp's `ExportKeys`
set only the key list's end marker's code; its qualifier and action (6
bytes, offsets 577-582 of a default file) came from `AllocVec`. Two Saves
now give the same file (before: 4 of the 6 bytes differed).

**Pixmap.mui without bzip2's compressor**: `bzlib.c`'s compressor behind
`BZ_NO_COMPRESS`, `blocksort.c` and `compress.c` not linked. Code hunk
37,976 -> 22,856 bytes, file 44,120 -> 28,708 (68000 build with symbols).

Checks: `make check` passes (four window checks' instance offsets moved
by 4); fullcheck (19 demos, 8 MUI-Demo sub-windows) all open and close,
no alert, the same memory figures as `lean`'s build in the same session;
real-program suite, MUI 3 programs with file requesters or settings
windows (cmanager, ro, yam25, amftp, multiren, archiver) as before.

## Every settings page fits 640x256; a window that grows past the screen (2026-11-07)

Branch `prefsfit`.

**Every page of Zune prefs fits a 640x256 screen at the default fonts.**
Measured with a `-DPREFS_SIZES` build (it now prints every object four
levels deep, and `ENV:PrefsPage=n` leaves page n shown) on FS-UAE
`a1200-31`'s 640x256 Workbench, topaz 8, thick frames (the screen is
2:1): a page may be 440 x 195 there (446 x 195 with thin frames, as on
the 640x1024 screen of `tests/lean/probe/pubscr.c`, where the first
numbers were taken). Page minimum sizes:

| page | before (thin frames) | after (640x256, thick) | what changed |
|---|---|---|---|
| Windows | 470 x 165 | 432 x 165 | "Frames:" for "Frame thickness:" |
| Groups | 478 x 180 | 414 x 163 | columns of their own widths; "Truncate titles:" beside its checkmark |
| Listviews | 486 x 133 | 422 x 129 | columns of their own widths, Design beside Control |
| Navigation | 467 x 154 | 434 x 154 | "Active:" for "Color of active object:"; the drag frame in the rows above "Look:" |
| TextEditor.mcp | 502 x 230 | 415 x 171 | the Settings tab split: Control, Design (two columns), Separator; a new tab "Cursor" (its catalog string) with Fonts and Cursor |
| NListviews.mcp | 536 x 272 | 428 x 192, still too tall (below) | Options split into Options, Drag&Drop and Miscellaneous (their catalog strings); the colour options on Miscellaneous; Layout beside the fonts; the two scrolling checkmarks in one row |
| NListtree.mcp | 442 x 221 | 394 x 172 | images and pens in two rows with the labels beside; the example's buttons one above the other |
| TheBar.mcp | 8 x 8 | 314 x 8 | Scrollgroup fix below |
| the other 9 | fit | fit (System 421 x 162) | |

Every setting is still there, with the same objects and the same
catalog strings; only three labels are new English text (the catalog's
were too long; translations of the other strings may still be wider).
On the 640x256 Workbench the window now opens and stays in topaz 8 on
every page but NListviews (below): the other pages no longer trigger Zune prefs' "open again smaller"
(kept as a safety net). Screenshots of all 17 pages were taken in the
session (`/tmp/prefsfit/shots3/page-N.png`, kept outside the repository).

**TheBar's settings page measured 8 x 8.** For MUI 3 (the version Zune
reports to a MUI 3.8 program) TheBar.mcp puts its settings into a
Scrollgroup with `MUIA_Scrollgroup_FreeHoriz` FALSE and
`MUIA_Scrollgroup_AutoBars` TRUE around a virtual group. Zune's
Scrollgroup with AutoBars took the virtual group's own minimum, 8 x 8,
also on the axis that does not scroll. MUI 3.8, measured with
`tests/lean/probe/sgfree.c` (a 300-pixel-wide, 640-pixel-tall column):
min 318 x 34 with or without AutoBars (it ignores that MUI 4 tag); Zune
gave 318 x 34 without and 8 x 8 with it. Now an axis without a scroller
takes the contents' whole minimum plus the other axis' scroller in both
cases: 318 x 8 with AutoBars (the height stays free: the bar appears when
needed). The TheBar page is 314 x 8 and shows its settings at full width.

**A window whose contents grow past the screen** (a library limit found
with Zune prefs). Measured black-box on MUI 3.8 (`tests/lean/probe/grow.c`:
a window with one button, then N "Button" buttons added with
InitChange/OM_ADDMEMBER/ExitChange, or a page of N buttons added to a
page group and shown; FS-UAE `a1200-31-mui38`, 640x256):

| N | MUI 3.8 | Zune before | Zune now |
|---|---|---|---|
| 10 (fits the screen, not below the window's top) | moved up just enough: 288,105 64x45 -> 288,64 72x192, same fonts | sized to 72 x 151 (Intuition stops at the screen's edge), contents cut | as MUI 3.8 |
| 15 (does not fit at topaz 8) | closed and opened again: fitted stage by stage (helvetica 9) and centred, 298,4 44x247 | 72 x 151, contents cut | as MUI 3.8, the window pixel-identical |
| 30 (fits no stage) | the window closes (MUIA_Window_Open FALSE) | 72 x 151, contents cut | as MUI 3.8 |

The page-group variant behaves the same. In Zune: WindowResize moves the
window (ChangeWindowBox) when the new size would leave the screen;
RecalcDisplay, when the new minimum does not fit the screen at all, pushes
a private MUIM_Window_Refit to the application's input loop (the change
may come from an object inside the window), which closes the window and
opens it again at its default size: the opening's fitting does the rest,
and the application keeps the higher stage for its later windows as at
an opening. Nothing happens if the window was closed in between. Stack:
Window_Dispatcher 28 bytes as before, RecalcDisplay 68 (88 before), the
new code not inlined (`-fstack-usage`); Scrollgroup's frames unchanged.

**Not finished: NListviews.mcp.** Its page is now 428 x 192 (the Layout
options beside the Fonts, Options keeps Multiselect and Balancing), but
on the 640x256 Workbench Zune prefs still opens in helvetica when that
page is shown (seen 2026-11-07 with the page at 195 and at 192 high): the
window's real vertical extra is larger than the 21 pixels the budget
assumed (TextEditor's root, 211 high, fits; 232 does not). The tallest
tabs are Scrolling (137) and Fonts (132) plus the 31-pixel crawling
banner; about 20 pixels less are still needed. The other 16 pages open
and stay in topaz 8.

Checks (FS-UAE, final library; NListviews.mcp's last change after them):
`make check` passes; fullcheck: MUI-Demo's 8 sub-windows and the 18 other
demos open and close, no alert, lost bytes as on `rtgfix` (608, 200,
AppWindow 264); demopix: 18 of 27 windows pixel-identical with MUI 3.8,
the same 9 differing as before; real-program suite, MUI 3 group, a subset
(yam25 with its settings and address book, amftp, cmanager, ro, multiren
with its settings, archiver, the NList, NListtree, TextEditor and TheBar
demo9 demos): all ok, the six programs' windows identical to `rtgfix`'s
run. The whole MUI 3 group (36 programs, about 70 minutes) was not run.

## The suite's last differences against MUI 3.8 (2026-11-07)

Branch `diffs`. Each cause measured black-box on FS-UAE `a1200-31-mui38`
with a probe in `tests/lean/probe/` (object dumps of the programs' windows
with `zspy` + `objdump`), then Zune made to give the same:

| program | before | MUI 3.8 | cause | fix |
|---|---|---|---|---|
| TheBar demo2's control window, demo3 | 399 wide | 381 | a horizontal Radio's entries 4 px apart (Zune prefs' radio spacing); MUI 3.8: 1, a button and its label 4, no setting (`radio.c`) | Radio: 1 and 4 for MUI 3 programs (MUI 4/5 programs keep the setting) |
| AmTelnet | "amtelnet.1", 640x248 at y 4 | "AMTELNET.1", 640x256 at 0 | MUIA_Application_Base reads back upper case on MUI 3.8 (`appbase.c`); the window's inner maximum took the bottom border with the size gadget (10) although AmTelnet's size gadget is in the right border (2): 8 px too low, the terminal lost a row | base name upper case; the maximum from the real borders |
| AmIRC's server window | 550x256 at 45,0 | 550x236 at 45,10 | Zune's Listtree (a List around an NListtree) took NList's default size (359 x 255); MUI 3.8's Listtree.mcc is sized as a List: 3/8 rows of at least 10 px, 40/100 wide; MUI 3.8's List also uses MUIA_List_MinLineHeight for those rows (`lvrows.c`) | List: rows from MinLineHeight when higher, a replaced entry area sized by List's rules; Listtree: rows of at least 10, MUIA_Font passed on |
| IBrowse 2.5.9 | no main window (min height 367) | 480x192 at 80,32 | its toolbar buttons (40x36 on MUI 3.8) measured 998x276: 248 px a character. MUI 3.8's render info holds every preset's font at offsets 32-60, all open by AskMinMax (`rpminmax.c`); IBrowse reads its font there, Zune had its pens there and opened presets only on use (248 = 0x00F8, what a UWORD read through a NULL font gives from the ROM vectors) | `mri_Fonts` right after the public fields; for MUI 3 programs every preset's font opened after each setup of the root |

IBrowse 2.5.9's window now has the same objects and sizes as on MUI 3.8
(its object dump differs only in a few maximum heights). The other item,
**Zune prefs' NListviews.mcp page** (192 high, Zune prefs fell back to
helvetica on 640x256 when it was shown): the credits crawl through one
line instead of three and the Scrolling tab lost two empty spaces: 428 x
171, Zune prefs opens and stays in topaz 8 with that page on FS-UAE's
640x256 Workbench (screenshot taken). Every setting is still there.

Stack (`-fstack-usage` against `lean`): no function grew. The new code
sits in helpers that are not inlined (WindowScreenMax 28 bytes,
OpenAllPresetFonts 36, AppBaseName 44, all shallow calls); Radio's setup
160 (168).

Not done: MUI-Demo's Listviews window (2 px narrower) and the group
spare-pixel distribution (item 5 of the session's list): no time left.
Seen on the way, not fixed: Zune has no ARexx port right after
ApplicationObject (`appbase.c`: MUI 3.8 has it at once); AmIRC's and
AmTelnet's logos (Bitmap.mui objects of the same size) stay empty on Zune.

Checks (FS-UAE, final library): `make check` passes; fullcheck: MUI-Demo's
8 sub-windows and the 18 other demos open and close, no alert, lost bytes
as before (608, 200, AppWindow 264); demopix: 18 of 27 windows
pixel-identical with MUI 3.8, the same 9 differing; real-program suite:
MUI 3 group 35 of 36 as on MUI 3.8 (was 30; SimpleMail windowless in
FS-UAE as before), MUI 4/5 group 5 of 5 as `lean`'s. Details in
[tests/lean/real-programs.md](../../tests/lean/real-programs.md),
"Update, branch `diffs`".

## The leftovers: ARexx port, logos, Listviews, group spare pixels (2026-11-10)

Branch `leftovers`, the four items left open by `diffs`, measured
black-box on FS-UAE `a1200-31-mui38` (and `a1200-32-mui5` where Zune
follows the program's MUI) with probes in `tests/lean/probe/`:

| item | MUI 3.8 (measured) | cause in Zune | fix |
|---|---|---|---|
| ARexx port | made by OM_NEW, named as MUIA_Application_Base reads back (LOWBASE.1, LOWBASE.2; SINGLE for a single-task one), also without Commands or a RexxHook; none with MUIA_Application_UseRexx FALSE; gone after dispose; the name is not checked against other programs' ports (a foreign PREBASE.1 gives a second PREBASE.1) (`rexxport.c`) | the port was made only with Commands or a RexxHook | made unless UseRexx FALSE |
| ARexx commands | a host's own CreateRexxMsg (`wasabi arexx PORT QUIT`) runs; a command it does not know answers -3, a bad INFO item or missing argument -4 | `IsRexxMsg()` is TRUE only for messages ARexx itself sent: every other host's command was replied unread (QUIT did nothing); unknown commands answered 10 | any RXCOMM message runs; -3 and -4 as MUI 3.8 |
| AmIRC's and AmTelnet's logos | drawn | Bitmap's remap read the source with ReadPixelLine8 through a one-plane temporary rastport; the graphics autodoc wants "a copy of rp", a plane for each: on OS 3.1 a 5-plane logo came out in pens 0 and 1 (grey with a few white dots) | the temporary row gets the source's depth (Bodychunk had the same one-plane row for WritePixelLine8) |
| MUI-Demo's Listviews window | 292 wide | a List's MUIA_InnerLeft replaces its frame's inner spacing on MUI 3.8, and a Volumelist has InnerLeft 4 unless given, with or without a frame (`vlinner.c`: add 6/4/2/4 for InputListFrame, + InnerLeft 2, + InnerLeft 0, no frame); MUI 5 has neither | for MUI 3 programs a List's inner spacing goes to its entry area (where Zune's frame is) and Volumelist asks for 4 on the left |

AmTelnet's logo tags were logged with `tagspy` (now `tagspy TAG...`):
a Bitmap of its own 50x37 5-plane chip BitMap, 32 SourceColors,
Precision 0; `bmlogo.c` makes the same and is pixel-identical with MUI
3.8 now. The Listviews window is 292x199 as on MUI 3.8 (6,227 differing
pixels -> 1,136): what is left is the lists' column widths (MUI 3.8 shows
the start of the Dirlist's date column and three letters of each volume,
Zune none and two) and the assign image.

Not matched (seen): with four applications in one task MUI 3.8 refuses a
fifth, single-task one, and Zune gives a sixth no port (both look like
running out of signal bits: *proposed*); MUI 3.8's INFO TITLE and VERSION
of an application without them give "Unnamed" and "$VER: Unnamed 0.0",
Zune "?".

**Group spare pixels: a rule for the test groups, not yet for every
group.** Fitted against `hdist-mui38.txt` and `vdist-mui38.txt` with a
script trying variants of MUI 3.9's way (which Zune has: 95 of the 644
rows off), then checked on two more MUI 3.8 runs of `tests/native/hdist.c`
made for it (`hdist s`/`vs`: the children 4 apart, `hdist d`/`vd`: the
window at its default size; `*-s4-mui38.txt`, `*-def-mui38.txt`). The
model, `tools/lean/groupfit.py`, gets 1,308 of the 1,312 rows:

1. First split: every child gets its weight's share of the whole,
   truncated (Zune: of what the children before it left), of the group's
   space less **twice** the minimum of each child of weight 0.
2. MUI 3.9's passes: clamp every child, give each child that can move its
   weight's share of the remainder over all children's weight, truncated;
   eight times.
3. Up to eight more passes that give **each** child that can move one
   pixel towards the remainder (so with equal weights the free children
   come out equal and a clamped child takes the odd pixel: `maxed` 345 is
   39 153 153, its max-40 child one below its maximum; `mins` 207 is
   54 53 100).
4. The rest a pixel at a time from the last child back.

Built into Zune (commit 0d821437) it gave the same 642 of 644 rows on
FS-UAE, Balancing came closer (1,085 differing pixels -> 204), but
MUI-Demo's Groups window (0 -> 2,113) and the Pages demo (0 -> 952) then
differed, Subtask more (450 -> 1,031): demopix 16 of 27 instead of 18, so
it was taken out again (3946c1cb). The counter-example, measured
(`groups-objdump-mui38.txt`): Groups' root VGroup, 186 pixels for four
children of weight 100 (Listview min 34 def 76; "Group Types" min = def
52; two rows fixed at 29): MUI 3.8 gives 46 82 29 29, the model 61 67 29
29, Zune's MUI 3.9 way the same as MUI 3.8. 46 is the Listview's first
share (186 / 4) and the "Group Types" child takes all the rest, so here
the remainder is not shared by weight at all. Next: test groups with two
fixed children and one clamped at its minimum, and nested frames.
Things tried that fit fewer rows: plain proportional with the rest to the
last or first child, sequential shares from the first or the last child,
floor instead of truncation for negative remainders (fits `mins`, breaks
`fix0`), the remainder over the movable children's weight only, one
extra pixel pass that stops at zero.

Stack (`-fstack-usage` against `lean`, the changed files): no function
grew. Bitmap's setup 320 -> 316; List_InnerToArea 16 is new, not inlined,
called from List's OM_NEW, whose frame is unchanged. (The group rule,
taken out again, had cost Layout1D_mui_redistrib 48 -> 52.)

Checks (FS-UAE, the final library of the branch): `make check` passes
(the ARexx harness now sends RXCOMM messages and expects -3 for a command
nobody knows); fullcheck: MUI-Demo's 8 sub-windows and the 18 other demos
open and close, no alert, lost bytes as before (608, 200, AppWindow 264);
demopix: 18 of 27 windows pixel-identical with MUI 3.8 (Listviews now the
same size, 6,227 -> 1,136 differing pixels; Layout 3,321 -> 3,154); suite
MUI 3 group: 35 of 36 as on MUI 3.8 (SimpleMail windowless in FS-UAE as
before; IBrowse 2.5.9's frozen and AmTelnet's reset second quit and
MultiRen's "still running" are the known FS-UAE and quit differences),
AmIRC's and AmTelnet's logos drawn; MUI 4/5 group: 5 of 5 as `diffs`'
run. Memory: each program's first run from boot 160-224 bytes more (its
ARexx port, which MUI 3.8 has too), second runs within 112 bytes. Library
file (stripped): 271,256 bytes. Details in
[tests/lean/real-programs.md](../../tests/lean/real-programs.md),
"Update, branch `leftovers`".

## Settings: against MUI 3.8's, and reading its files (2026-11-10, phase 7)

Branch `prefs38`. The full inventory, the file format and what is left:
[settings-mui38.md](settings-mui38.md). In short, all measured black-box on
FS-UAE `a1200-31-mui38`:

- Zune prefs already offered every setting of MUI 3.8's settings program
  except the pages of two third-party classes MUI 3.8 ships (Listtree,
  Busy). The library ignored the ARexx switch, the drag settings and the
  iconification settings (now honoured, below), the cycle popup speed, the
  middle-button drag, the drag look and the active object colour (still).
- MUI 3.8's files are Zune's own format (Zune took its ids from them) with
  differences found by changing one setting at a time: ten items under
  other ids (System page, frame thickness, refresh) and the title gadgets
  as four items, frame types 5-10 numbered differently, brushes as `3:`,
  window positions in a `MUIW` chunk. MUI 3.8 writes only what differs from
  its defaults.

| commit | what | library |
|---|---|---|
| 91bf8549 Zune prefs | "Import MUI 3.8 settings..." in the Project menu, and a question when Zune has no settings yet and ENVARC:MUI has MUI 3.8's: the global file into the editor (tested; Save or Use keeps it), each program's file into ENVARC:zune and ENV:zune unless Zune has one; pictures that cannot be found left out; the MUI files only read. `tests/check_mui38_import.py` runs the converter on the PC | none (Zune prefs +2.9 KB) |
| fb82c055 muimaster | the ARexx switch (no port when off) and the drag settings (autostart and its distance, the left-button qualifier) honoured; defaults as before | +136 bytes |
| 4b98b612 muimaster | the iconification settings honoured (icon, Tools menu item, iconified at start, the hotkey; measured on MUI 3.8 first); MUIM_Application_NewInput returns the commodities port's signal too (Exchange's commands and the hotkey waited for other input) | +412 bytes (library file, stripped: 271,256 -> 271,804 bytes for the branch) |

Tests with the branch's last library (FS-UAE, 2026-11-10): `make check`
(with the new `check_mui38_import.py`); fullcheck: MUI-Demo's 8
sub-windows and the 18 other demos open and close, no alert, lost bytes as
before (608, 200, AppWindow 264); demopix: 18 of 27 windows pixel-identical
with MUI 3.8, as `leftovers`; suite MUI 3 group: 35 of 36 as on MUI 3.8
(SimpleMail windowless in FS-UAE as before, also run again on its own with
the other late programs; IBrowse 2.5.9's frozen second quit and MultiRen's
"still running" are the known differences); MUI 4/5 group: 5 of 5 with the
same results as the earlier Zune run. Zune prefs' window unchanged (the
import is a menu item and requesters). Stack: no new deep paths (the drag
check is inlined into the motion handler).

## MUI 4/5 programs: a wider test set and what they missed (2026-11-10, phase 6)

Branch `mui45`. The rule (our partners'): MUI 3, 4 and 5 programs run as fully as possible,
but a MUI 3 program never pays for MUI 4/5: every MUI 4/5-only piece is a file loaded on
demand (a class file or a module) or behind the per-program identity, and a MUI 3 program
loads none of it; its memory, speed and the library's code do not grow.

**The test set** (tests/lean/real-programs.md): the MUI 4/5 group grew from 5 to 24 entries.
Programs that need MUI 4/5 on a 68k Amiga are still rare (Aminet's readme search for
"MUI 5", "MUI 4", "MUI 3.9"); most want MUI 3.8 and use MUI 4/5 extras. Added: MUI 5's own
15 demos that MUI 3.8 does not have (from MUI-5.0-20210831-os3.lha, whose `SDK/MUI` also has
the autodocs and the demos' sources), PassPocket 1.1 (MUI 3.9+), Ticklish 1.1.1 (made with
Hollywood), the TabGroup.mcc 19.2 demo and LumiZip 1.0 (both 2026). LayGroup and GIFAnim
come with MUI 5; Zune and MUI 3.8 get the classic Aminet classes. What each asks for
(snooped): the MUI 5 demos and PassPocket version 20 (so Automatic gives them MUI 4),
TabGroup 19, Ticklish 0, LumiZip 11 (MUI 3.8).

**What they missed, and where it went** (black-box against MUI 5 and its public autodocs,
SDK headers and ChangeLog; never MUI's code):

| commit | what | where (MUI 3 programs) |
|---|---|---|
| 29b037f1, b3e001f4 | Hyperlink.mcc (MUI 5 brings it; MUI 3.8 has none): a Text that follows its URI when clicked, link look (underlined, the fill pen: blue on a 4-colour Workbench, as MUI 5) or a button when made with a frame; about:, amigados:, workbench: in the class, the rest through openurl.library; HoverTip, Copy, Encode, Decode. MUI 5's Hyperlink demo ended at once on Zune | class file `LIBS:MUI/Hyperlink.mcc` (10.6 KB), opened only by name |
| d766f14a | MUI 5's "Z" compressed images (zlib streams) in Pixmap.mui and Rawimage.mcc, by a small inflate written from RFC 1950/1951 (checked on the PC against zlib: stored, fixed and dynamic blocks, levels 1/6/9). MUI 5's EmbeddedImages demo ended ("Is z.library missing?") | Pixmap class file +1.8 KB (Pixmap is a MUI 4/5 class) |
| e6f5c774 | MUI 5's answer to a window larger than the screen (its ChangeLog, 5.0-2018R1; its Pointers demo opens 640x256 with scrollers): the root goes into a Scrollgroup with a Virtgroup for that window only, MUI 3.8's smaller stages stay the fallback; a new root object disposes the added Scrollgroup; MUIA_Window_RootObject still gives the program's root. Only for programs that get MUI 5 | the groups are made and taken apart in the title gadget module (Zwintitle.mui +408 bytes); the library keeps the decision in the fitting loop |
| 592f40ce | an elided text (MUIA_Text_Shorten) has no minimum width but the whole text by default (MUI 5's ElidedText demo opens with every line whole; Zune gave three W's) | Text, a path only MUIA_Text_Shorten takes (+4 bytes) |
| d71e900a | Textdata to Latin-1: U+2018/U+2019 as \` and ', U+2013/2014 as -, U+201C/201D as " (MUI 5's Textdata demo; Zune showed ?) | Textdata class file |
| fe4c589d | **a MUI 3.8 fix** found by TabGroup.mcc (MUI 3.8+): Group's OM_NEW added its Child objects by sending OM_ADDMEMBER through the object's own class; TabGroup forwards that to a page group it has not made yet, so every child was lost (an empty line under each title). MUI 3.8 and MUI 5 show the tabs: Group now adds them itself (CoerceMethod). The demo is now 166x143 as on MUI 3.8 | library, same size |
| 0f2df272 | suite: Aminet LayGroup and GIFAnim for Zune and MUI 3.8, TabGroup.mcc for all | - |
| c61d64ba, 39335297 | to keep the library from growing: the Settings and Settingsgroup classes (made only by settings programs) became class files. Mccprefs was tried too and kept in the library: the LoadSeg trace showed AmIRC, a MUI 3 program, loading Textinput.mcp and NListviews.mcp, whose classes are Mccprefs subclasses | `LIBS:Zune/Settings.mui`, `Settingsgroup.mui` |

**Proof that MUI 3 programs pay nothing** (FS-UAE A1200 OS 3.1, 68000 build, GCC 16.2;
`lean` at 6de7fb8f against this branch's last library, the same session):

- **Library file** 271,804 -> 271,132 bytes (-672): the MUI 5 additions cost +200 bytes in the
  library (the scrollable-window decision, the Text path, the glue to the title gadget module),
  the two settings classes moved out -872. Library alone in memory (`libonly.sh`, 4 MB fast):
  285,136 -> 284,536 bytes.
- **No MUI 4/5 file loaded by a MUI 3 program**: a LoadSeg trace (`snooplog.sh`) of the whole
  MUI 3 group (36 entries) and of fullcheck: no Hyperlink.mcc, Zwintitle.mui, Pixmap.mui,
  Rawimage.mcc, Textdata.mui, Zlist.mui, Settings.mui or Settingsgroup.mui. The trace is how
  Mccprefs was caught: with it as a file, AmIRC and AmTelnet loaded it (through Textinput.mcp
  and NListviews.mcp); with it back in the library AmIRC's trace is `lean`'s set.
- **Memory**: MUI-Demo (`measure.sh` twice each, 4 MB fast): first program 478,616 -> 478,016
  bytes, each further 204,520-204,584 on both. Real programs without a trace running (a `wasabi
  snoop` attached during a run adds tens of KB to the in-use figures: measured, AmigaGPT +110 KB,
  NewsCoaster +2.9 KB): AmigaGPT, NewsCoaster, ViewCSV, YAM 2.5 second-run in use the same to
  48 bytes, first runs 600 bytes less (the library).
- **Speed**: MUI-Demo's first window 1.60-1.62 s (`lean`) and 1.44-1.50 s (this branch), within
  the emulator's spread; no MUI 3 path got code except Group's OM_NEW, which calls Group's own
  dispatcher instead of the object's (one call less).
- **Stack** (`stackrun.sh`, twice each): MUI-Demo 5,388, Layout 2,268, Pages 2,240, Slidorama
  2,304 bytes on both; Virtual varies from run to run on both libraries (lean 2,408-2,672, this
  branch 2,672-2,736).
- `make check` passes. fullcheck: MUI-Demo's 8 sub-windows and the 18 other demos open and
  close, no alert, lost bytes identical to `lean`'s (608, 200, AppWindow 264), the whole output
  identical. demopix: 18 of 27 pixel-identical with MUI 3.8, every window's figure as
  `prefs38`'s (Layout, not comparable, varies). MUI 3 group: 35 of 36 as `lean`'s run (windows,
  status), SimpleMail windowless in FS-UAE as always (reset or frozen at its quit).

**Results, the MUI 4/5 group** (Zune before = `lean`'s library, after = this branch; "Auto":
each program gets the MUI it asks for; MUI 5 = FS-UAE a1200-32-mui5, MUI 5.0-20210831):

| ID | MUI 5 | Zune before, Auto | Zune after, Auto | Zune before, MUI 5 forced | after, MUI 5 forced |
|---|---|---|---|---|---|
| yam210 | ok, reset at quit 376x201 | ok 400x98 | ok 400x98 | - | - |
| ibrowse30 | ok 480x192 | ok 480x192 | ok 480x192 | - | - |
| dizzytorrent | ok 372x256 | ok 378x256 | ok 378x256 | - | ok 378x256 |
| amigagpt | ok 576x230 | ok 590x253 | ok 590x253 | - | ok 590x255 |
| mqttexplorer | exited | exited | exited | - | - |
| mui5-aboutbox | ok 208x53 | ok 200x39 | ok 200x39 | ok 200x39 | - |
| mui5-animals | ok 328x61 | ok 320x47 | ok 320x47 | ok 320x47 | - |
| mui5-calltips | ok 304x65 | ok 296x52 | ok 296x52 | ok 296x52 | - |
| mui5-class4 | ok 296x85 | ok 288x65 | ok 288x65 | ok 288x65 | - |
| mui5-clock | nowindow,busy 224x249 | ok 184x77 | ok 184x77 | ok 184x77 | - |
| mui5-elidedtext | ok 599x189 | ok 154x141 | ok 590x141 | ok 154x141 | ok 590x141 |
| mui5-embeddedimages | exited | exited | exited | exited | window 640x256 (after 150 s) |
| mui5-gifanim | ok 388x98 | exited | ok 394x53 | exited | ok 394x53 |
| mui5-hyperlink | ok 436x125 | exited | ok 438x105 | exited | ok 438x105 |
| mui5-laygroup | ok 294x207 | exited | ok 300x102 | exited | ok 300x102 |
| mui5-list1 | ok 344x181 | ok 327x132 | ok 327x132 | ok 327x132 | - |
| mui5-menuimages | ok 408x93 | ok 400x83 | ok 400x83 | ok 400x83 | - |
| mui5-pointers | ok 640x256 | ok 364x248 | ok 364x248 | ok 364x248 | ok 640x246 |
| mui5-textdata | ok 180x81 | ok 174x68 | ok 174x68 | ok 174x68 | ok 174x68 |
| mui5-title | ok 358x177 | ok 490x117 | ok 490x117 | ok 490x117 | ok 490x117 |
| passpocket | ok 254x101 | ok 313x87 | ok 313x87 | ok 313x87 | ok 313x87 |
| ticklish | ok 253x81 | ok 258x56 | ok 258x56 | ok 258x56 | - |
| tabgroup-demo | ok 172x165 | ok 166x81 | ok 166x143 | ok 166x81 | ok 166x143 |
| lumizip | ok 505x256 | ok, reset at 2nd quit 611x256 | ok, reset at quit 611x256 | ok 627x256 | ok 627x256 |

"after" in the Automatic column is this branch's library from before the last two
commits (the settings classes; they touch no program here), the MUI 5 forced column the final
one. Sizes are the first window's, screen pixels. MUI 5 uses its own settings (helvetica,
other spacing), so most windows differ by a few pixels on every line; that is the look, not a
fault. What the lines say:

- **Fixed**: Hyperlink (Zune's new class), GIFAnim and LayGroup (the Aminet classes in the
  suite; LayGroup 19.2 shows the demo's window but none of its 30 objects: the demo needs MUI
  5's newer LayGroup), ElidedText (whole lines), the TabGroup demo (its tabs and pages),
  Pointers with MUI 5 forced (640x256 with scrollers, as MUI 5), EmbeddedImages with MUI 5
  forced (its window opens, 640x256 with scrollers; it unpacks and dithers its pictures for over
  two minutes in FS-UAE, so the suite still counts it "exited"; by hand it opens; before, it
  ended with "Is z.library missing?"). Textdata's quotes (not visible in the table).
- **Automatic gives the MUI 5 demos MUI 4** (they ask for version 20), so Pointers and
  EmbeddedImages get MUI 3.8's smaller stages, not MUI 5's scrollers; with "Behave as MUI 5"
  (or a per-program MUI5 line) they get MUI 5's behaviour.
- **Same on both**: Ticklish stops at Hollywood's "Error allocating bitmap!" on the AGA
  machines (MUI 5 too); MQTTExplorer ends at once; PassPocket opens its password window.
- **On MUI 5, not on Zune**: the Clock demo never opened its window on MUI 5 (busy), EmbeddedImages
  ended at once there; YAM 2.10dev reset MUI 5's machine at quit (known).
- **Open** (fixed on `mui45b`, next section): LumiZip 1.0 resets the OS 3.1 machine when it quits on Zune with the MUI 3.8 identity
  (4 of 4 suite runs, `lean`'s library too), never with MUI 5 forced, never on MUI 3.8 or MUI 5;
  three quits by hand with `trapcap` running were clean and the suite run with trapcap recorded no
  exception and no alert. Cause not found.
- **Seen, not done** (MUI 4/5 look and features left for later, with what uses them):
  Title's tabs are flat boxes where MUI 5 draws register tabs, and the demo's animated
  "throbber" Pixmap does not show on the 4-colour screen; List's sort mark on a SORTABLE
  column's title (MUI 5 List1); MUIM_FindObject (IBrowse 3.0a uses it), MUIA_FrameDynamic and
  MUIA_FrameVisible (YAM 2.10dev), MUIM_Mccprefs_RegisterGadget (the popup menu on mcp
  gadgets: YAM's and IBrowse's mcps; harmless without it), MUIA_Application_UseScreenNotify
  (LumiZip), MUIM_Application_BuildSettingsPanel (Animals' settings). Found by scanning the
  programs' executables for the SDK's ids that Zune's sources lack (a hint, not a trace).

Tools on the way (not committed; in this session's /tmp/mui45/scripts): a side-by-side crop of a
program's windows from two suite runs, the id scan, a LoadSeg logger that survives reboots.

## LumiZip's reset found; MUI 4/5 calls programs use (2026-11-11, phase 6)

Branch `mui45b` (from `lean`). The rule is the same as in the section above: MUI 3, 4 and 5
programs run as fully as possible, and a MUI 3 program pays nothing for MUI 4/5.

**LumiZip 1.0's reset at quit** (open since `mui45`: it reset the machine at quit with the MUI 3.8
identity in 4 of 4 suite runs, never with MUI 5 forced, never on MUI 3.8 or MUI 5). Found and
fixed (e3e69e1e):

- **Caught**: a loop that starts and quits LumiZip on one boot (no reboot between rounds, `trapcap`
  running) reset in round 11, 1, 1 and 1 of four tries. The trap record: an illegal instruction at
  a garbage address, reached from `CloseLibrary()` called by `MUI_FreeClass()`, called by
  `MUI_DeleteCustomClass()` (the return addresses on the stack, matched in the library's
  disassembly). A temporary debug build (RawPutChar, `wasabi debug`) printed every
  `MUI_CreateCustomClass()` call: LumiZip makes an Area subclass with supername "Area.mui" **and
  garbage in A2 (supermcc) and A0 (base)**. Zune took A2 as the superclass descriptor, so the
  new class's superclass was the name string itself; at quit `MUI_DeleteCustomClass()` passed that
  "class" to `MUI_FreeClass()`, which closed its dispatcher data as a library: a jump into
  garbage. What the garbage pointed at changed from run to run, so most quits survived; the class
  is never used for an object, so nothing showed while LumiZip ran.
- **What MUI does** (black box, `tests/lean/ccprio.c`: a class with both a supername and a
  supermcc): MUI 3.8 (19.35) and MUI 5 (21.227) both take **the name**. Zune does too now. And
  `MUI_DeleteCustomClass()` gives back only a superclass it took by name (a supermcc's class is
  borrowed from its owner; a private flag after `mcc_Module` remembers which). Library +24 bytes;
  each custom class +4 bytes (the flag).
- **Proof**: the same loop with the fix: 16 of 16 quits clean, then 10 of 10 with the final
  library; `ccprio` on Zune prints MUI's answer ("super: name").
- Why only with the MUI 3.8 identity: not traced; with MUI 5 forced LumiZip probably does not
  make that class (its window differs too, 627 against 611 pixels wide).

**MUI 4/5 calls the programs use** (from the id scan of the section above), black-box against
MUI 3.8 and MUI 5 with `tests/lean/m45probe.c`:

| commit | what | MUI 3 programs pay |
|---|---|---|
| 055933c5 | **MUIM_FindObject** (IBrowse 3.0a): the found object or 0. MUI 3.8 answers it from a Group and the object itself only (from a Window or Application: 0); MUI 5 also from Window (its root) and Application (its windows). Zune now gives MUI 5's answers everywhere (m45probe's lines are MUI 5's): it walks the tree as MUIM_FindUData does, only the comparison differs | library +84 bytes (MUI 3.8 has the method too) |
| 6b487894 | **MUIA_FrameVisible / MUIA_FrameDynamic** (YAM 2.10, ISG BOOL, defaults TRUE/FALSE as MUI 5 answers them). FrameVisible FALSE draws the frame as a phantom (room kept, not drawn). FrameDynamic is kept and answered; showing the frame while the pointer is over the object is not done | library +250 bytes (six switch cases in Area and a draw test) |
| 1418ad37 | **List's sort mark** (MUI 5 List1): an outlined triangle after the sort column's title, up for ascending, down for descending, and room for it in a SORTABLE column's title | drawing in the list module `Zlist.mui` (+312 bytes), called only for a SORTABLE column (MUI 3.8's format has no SORTABLE); library +172 bytes (the call, the room, the glue) |
| bb090f6f | **Title's tabs** drawn as register tabs (MUI 5's Title demo): shine left and top with cut corners, shadow right, the active tab 2 pixels higher and open at the bottom | class file `Title.mui` +336 bytes; library unchanged |

- **MUIA_Application_UseScreenNotify** (LumiZip, [I..]): FALSE asks MUI not to close and reopen the
  windows when the Workbench screen closes. Zune never does that (no screen notification on OS 3.x
  here), so a program gets what FALSE asks for; nothing was added.
- **Not done**: MUIM_Mccprefs_RegisterGadget's popup menu (Reset to defaults / Last saved /
  Restore on mcp gadgets: needs the prefs program's dataspaces; the method still answers 0, which
  the mcps ignore); FrameDynamic's hover; Title's white inactive tabs and the frame round the
  pages; MUIM_Application_BuildSettingsPanel.

**THE RULE, measured** (FS-UAE A1200 OS 3.1, 68000 build, GCC 16.2):

- **Library file** 271,132 (`lean`) -> 271,676 bytes (+544): the LumiZip fix 24, FindObject 84
  (a MUI 3.8 method), FrameVisible/FrameDynamic 250, the sort mark's call and room 172. So a MUI 3
  program carries about 420 bytes of library code for MUI 4/5 features it never calls; it runs
  none of it but a flag test per Area draw and a column test per title cell. **This bends the
  rule**: the Area attributes are the candidate to pay back (they could move into a module the way
  Zwintitle did, or something else could leave the library), left for the next round.
- **No new file and no new load for MUI 3 programs**: the sort mark's module is the one List
  already had for drag and drop and in-place editing, and it is called only for a SORTABLE column;
  Title.mui is loaded only by programs that make a Title (MUI 4/5).
- **Stack**: no new recursion or locals on the MUI 3 paths (one LONG in List's title width).

**Results** (final library 271,676 bytes, commit of Title's tabs; FS-UAE A1200 OS 3.1):

- `make check` passes.
- **LumiZip**: 26 of 26 start/quit rounds clean on one boot with `trapcap` running (16 with the
  fix alone, 10 with the final library); before the fix, resets in round 11, 1, 1 and 1. In the
  suite's MUI 4/5 group: "ok", both quits clean (before: "reset at quit").
- **fullcheck**: the whole output identical between the first and the final library of this
  branch, and `lean`'s figures: MUI-Demo's 8 sub-windows
  and the 18 other demos open and close, lost 608, 200 each, AppWindow 264, no alert.
- **demopix**: 18 of 27 pixel-identical with MUI 3.8, every window's size as before (Layout and
  InputHandler not comparable, as always).
- **MUI 3 group**: 35 of 36 as `mui45`'s run: every window's title and size identical, the
  second run's memory in use identical (to 48 bytes; NewsCoaster 2.9 KB and ViewCSV 552 bytes
  less), SimpleMail windowless and reset at its quit as always in FS-UAE. The first-run figures of
  this session are 15 KB above `mui45`'s session for every program, `lean`'s library included
  (thebar-demo1: `lean` 589,184, this branch 589,680 bytes in the same session: +496, the library).
- **MUI 4/5 group** (Automatic): all 24 as `mui45`'s "after" column, window sizes identical;
  LumiZip now quits cleanly; IBrowse 3.0a "frozen at 2nd quit" as in `mui45`'s run (FS-UAE
  itself crashes there, see "Finding why an OS 3.1 machine resets", Amiberry is clean).
- **Stack** (`stackrun.sh`, twice each, the same session): Layout 2,268, Pages 2,240, Slidorama
  2,304 bytes on both; MUI-Demo 5,388 or 5,420 on `lean` (three runs), 5,420 on this branch (two
  runs); Virtual varies on both (2,408-2,732 and 2,680-2,740).
- **Library alone in memory** (`libonly.sh`, 4 MB fast): 284,488 -> 285,080 bytes (+592; the file
  grew 544).

Scripts of this session (in /tmp/mui45b/scripts, not committed): `lzloop.sh` (start/quit rounds
on one boot with trapcap, saving the trap record and the FS-UAE log at a reset), `cmp.sh` (two
suite runs line by line), `findbytes.py` (a trap record's code bytes in the files on the host).


## A smaller library, second round (2026-11-11, phase 3)

Branch `smaller2` (from `lean`). First the ~420 bytes of MUI 4/5 code that `mui45b` put in the
library came back out ("this bends the rule", above), then more rarely used code left it. Same
rule as the first round: the library keeps what layout and the common path need and the decision
whether a feature is used at all; a program that does not use it never loads the file. Two moves
were taken back when the suite's LoadSeg trace showed a MUI 3 program loading the new file.

| commit | what | library file | file |
|---|---|---|---|
| 4e11580a Modules | the library's names for the modules' functions (`modules.c`) were 45 C functions of ~80 bytes each (look the module up, call it, give the class back). Now each is a 10-byte stub, `move.l #code,d0` / `bra.w zune_modcall`, and one routine in assembly does the rest with the caller's own arguments. The code holds the number of arguments, "fail when the first argument is NULL", the function's offset in the module's table (checked against the module structures at compile time), the module, and the value returned when it cannot be loaded. `modules.o` 3,732 -> 1,560 bytes; seven functions with logic of their own stay C | -2,200 with the next line | |
| 4e11580a Area | MUIA_FrameVisible / MUIA_FrameDynamic (`mui45b`, YAM 2.10) into a new module, Zmui45.mui (`mui45.c`); Area keeps two case labels per method and one call | (above) | Zmui45 1,404 |
| 11445ec2 Configdata | its setters MUIM_Configdata_SetULong ... SetString (`configset.c`) into a new module, Zprefs.mui; the default tables stay (`classes/configdata_private.h` describes them for both) | -920 | Zprefs 1,756 |
| 13d90c8b Classes | Aboutmui and Crawling as class files, as MUI 3.8 ships them (MUI:Libs/mui/Aboutmui.mui, Crawling.mcc) | -1,184 | Aboutmui 1,848, Crawling 1,900 |
| d3e745c7 String | the Textinput values String only keeps and answers (Changed, ResetMarkOnCursor, IsNumeric, SetMin/Max/VMin/VMax, MinVal, MaxVal, WordWrap, Styles, Font) and MUIA_String_Integer64 with its 64-bit decimal conversion into Zmui45.mui (String's instance data in `classes/string_private.h`); the marks and everything the editing uses stay. String's OM_SET also no longer calls a function for every tag it does not know (a `default:` did) | -728 | Zmui45 2,376 |
| 96ae4baf Area | **found by the trace**: TheBar demo10, a MUI 3 program, loaded Zmui45.mui. TheBar's TheButton.mcc sets MUIA_FrameVisible FALSE whatever MUI it runs on (`mcc/button/class.c`, "NtRaiseActive"). MUI 3.8 does not know the two attributes (GetAttr 0, the set ignored), so a program that gets MUI 3.8 now gets exactly that and no module (demo10's screenshot is pixel-identical either way); a program that gets MUI 4 or 5 gets them from the module (`m45probe 21`: the same lines as before; `m45probe` alone now prints MUI 3.8's "get 0") | +56 | |
| a541f90c String, Configdata | `-fstack-usage` against `lean`: String's OM_GET frame had grown by 8 bytes and Configdata's dispatcher by 4 (the module calls' arguments); each call now sits in a small function of its own | +68 | |
| 9665ed4f Configdata | **found by the trace**: AmTelnet (MUI 3) loaded Zprefs.mui at start: MUIM_Application_SetConfigItem, which MUI 3.8 has, calls MUIM_Configdata_SetString. SetString is back in the library; the module keeps the five setters only the prefs program uses | +132 | |
| 6528c28e Classes | **found by the trace**: ViewCSV (MUI 3) loaded Aboutmui.mui at start (it makes its About MUI object then; MUI 3.8 would load its own file there too). Aboutmui is built in again; Crawling stays a file (no program of the suite or the demos makes one) | +500 | Crawling 1,900 |

List's sort mark (`mui45b`) keeps its test, its room and its call in the library: it is called
only for a SORTABLE column, which MUI 3.8's format does not have; its glue is now a 10-byte stub.

**Library file 271,676 -> 267,400 bytes** (-4,276; before `mui45b` it was 271,132, so the MUI 4/5
code of that round is paid back and 3,732 bytes more). Code and data in the link 254,984 ->
250,728 bytes. New files: Zmui45.mui 2,376, Zprefs.mui 1,756, Crawling.mui 1,900.

MUI-Demo, cold, OS 3.1 + 4 MB fast (FS-UAE, 68000 build, GCC 16.2; `m.sh`, two `measure.sh`
runs and `libonly.sh` each, `lean` and `smaller2` in the same session; emulator figures, bytes):

| | library file | library alone | first program | each further | first window |
|---|---|---|---|---|---|
| lean | 271,676 | 285,128 | 478,560 | 204,520-204,584 | 1.42-1.44 s |
| smaller2 | 267,400 | 280,808 | 474,312 | 204,520-204,584 | 1.42-1.44 s |

The library is in memory once: the 4.3 KB come off the library alone and the first program;
each further program and the first window are as before.

Stack (`stackrun.sh`, twice each, side by side): MUI-Demo with its eight sub-windows 5,420 ->
5,404 bytes, Layout 2,268, Pages 2,240 (`lean` once 2,268), Slidorama 2,304 as before, Virtual 2,680
-> 2,676. `-fstack-usage` of every library function against `lean`: none larger; String's OM_SET
120 -> 104, OM_NEW 112 -> 108, Area's dispatcher 136 -> 132; the assembly stub routine holds 20
bytes plus the arguments it copies, less than the C functions it replaced (16-92 bytes each).

perfbench (68EC020, `bench.sh`, 15 repetitions, `lean` and `smaller2` on two instances, then
swapped): create+dispose -2.0 / +0.9%, open+close -3.3 / +0.4%, redraw -0.1 / -0.0%, relayout
-0.8 / -0.5%, list insert -0.6 / -0.1%, list active -1.1 / -0.3%, notify -0.5 / -0.4%, text set
-0.1 / -0.1%: no batch slower beyond the spread of two instances.

**Which files a MUI 3 program loads** (`wasabi snoop` through every run, `snooplog.sh`; the
final build): fullcheck (MUI-Demo with its eight sub-windows and the 18 other demos) loads the
class files `lean` loads and no module. The MUI 3 group (36 programs) loads only
`lean`'s modules where `lean` loads them (Zrequest.mui in MUIMine, Archiver, NewsCoaster, RO;
Zpicture.mui in the NList and TextEditor demos): no Zmui45, Zprefs or Crawling. It was the trace
that found TheBar demo10, AmTelnet and ViewCSV above. The MUI 4/5 group loads the same files as on
`lean` too (Zlist.mui only in MUI 5's List1, for its sort mark).

Checked (final build unless said):

- `make check` after every commit (`check_string_selection.py` now hands out the MUI 4/5 module's
  table in its harness: String's Integer64 goes through the module).
- fullcheck: MUI-Demo with eight sub-windows and the 18 other demos open and close, the whole
  output identical to `lean`'s (lost 608 / 200 / AppWindow 264, no alert), on four builds of the
  branch.
- demopix: 18 of 27 pixel-identical with MUI 3.8, every window's size and count as `mui45b`'s
  (Layout places its buttons at random).
- MUI 3 group: 35 of 36 as `mui45b`'s run, windows and sizes identical, the second run's memory in
  use identical (to 48 bytes), SimpleMail windowless and reset at quit as always in FS-UAE.
- MUI 4/5 group (24): all as `mui45b`'s, windows identical, except YAM 2.10, which showed an extra
  "Error messages" window (YAM's own: a theme picture it could not load through datatypes) in all
  three group runs of this branch. Run alone, it is exactly `lean`'s (no error window, the same
  windows, the same memory lost, -392 bytes), and `lean` alone the same: not traced further; it
  came in the group runs with five instances busy. (Found on `listslim`: it was the LoadSeg
  trace attached to the group runs, not Zune; "List made smaller" below.)
- `tests/lean/string45.c` (String's Integer64 init/set/get with the 64-bit extremes and the twelve
  Textinput values), `tests/lean/cfgset.c` (every Configdata setter, set and back to its default),
  `m45probe` (MUI 3.8 identity and `m45probe 21`): the same output as before the moves (MUI 3.8
  identity: FrameVisible/FrameDynamic now "get 0", as MUI 3.8).
- `tests/lean/smaller.c`'s pop-up menu strip (Zmenu through the new stubs) pixel-identical to
  `lean`; `tests/lean/aboutcrawl.c` (a Crawling object and the About MUI window) pixel-identical
  to `lean`.

Not done or not taken: `-fomit-frame-pointer` changes nothing with GCC 16.2 at `-Os` (the -2.7 KB
measured before was GCC 6.5). A drag in the DragnDrop demo with `tests/lean/mdrag.c` did not start
on `lean` either in this session (the press selected an entry, no drag image), so the drag module
was not driven by hand; its stubs share the one routine that the menu, requester, picture, list
and MUI 4/5 calls exercise.

**What is left toward 166 KB** (per object file, code and data): List 28.9 KB, Window 25.9, Group
21.6, String 19.5, Area 16.5, Application 16.0, text engine 10.5, vector images 8.2, frames 6.8,
Configdata 6.6. What could still leave without a MUI 3 program loading it on its normal path is
small: the trace showed that even "rare" things (About MUI, SetConfigItem, a MUI 4 frame
attribute) are on some MUI 3 program's start-up path; Window's snapshot runs at every quit,
Virtgroup is used by every Scrollgroup, the pointer code by every string gadget. About 16.5 KB of
the file are relocations (4,140 absolute references: ~2,300 into the code, ~1,800 to the library
bases and other globals): they cost disk, not memory. The rest means rewriting the big classes
smaller (roadmap "Later").


## Settings the library ignored: popup speed, active object colour (2026-11-11, phase 7)

Branch `setleft`. Measured black-box on FS-UAE `a1200-31-mui38` first (details in
[settings-mui38.md](settings-mui38.md#popup-speed-and-active-object-colour-2026-11-11-branch-setleft);
new test tools `tests/lean/setleft.c` and `tests/lean/mpress.c`):

| commit | what | library |
|---|---|---|
| 3fbcad3b Window, Area | the active object's frame in the settings' "colour of active object" (MUI 3.8: the same solid line one pixel outside, in that pen; shine by default). Pixel-identical with MUI 3.8 at the default and at `m5` | +132 bytes |
| 94fa0bfb Cycle | the popup speed: MUI 3.8 opens the popup at once at every speed and rolls it out from the top in about speed * height / 400 frames (0: at once, the default). Zune draws it a band of rows a frame through the menu module (Zmenu.mui, loaded only when the speed is above 0); at 50 rows 14/30/55 after 0/2/6 frames against MUI 3.8's 1/3/7 | +162 bytes (Cycle 152, stub 10) |

Library file (stripped) 267,400 -> 267,716 bytes. Unregistered MUI 3.8 ignores the Cycles page's
popup control (position, level, speed) in its files; its settings program's test mode applies
them to running programs. Not done: the middle-button drag and the drag look (no drag starts
from synthetic input in FS-UAE on MUI 3.8, so nothing could be measured), YAM 2.10's "Error messages" window in the MUI 4/5 group runs (explained on `listslim`: the LoadSeg trace).

Checked: `make check` (`check_window_pens.py` knows the extra pen request); fullcheck (below);
the popup at speeds 0, 10, 30, 50 and the active frame on a1200-31 with the branch's library.

## Settings pages for Listtree and Busy (2026-11-11, phase 7)

Branch `ltbusy`. The two outside classes MUI 3.8 ships with got their pages in Zune
prefs, as class files like NListtree.mcp (`LIBS:MUI/Listtree.mcp` 8 KB and
`Busy.mcp` 8.8 KB, built by `tools/imageclass`; **the library did not change**, no
source of it was touched). MUI 3.8's item ids were measured first (its settings
program on `a1200-31-mui38`, one setting at a time, the file dumped; ids, ranges
and defaults in [settings-mui38.md](settings-mui38.md)).

| what | in short |
|---|---|
| `Busy.mcp` | the sample bar (live), Color 1 / 2, Style (Old, Queue, KnightRider, StarTrek), Speed (1-250 ms, 251 = OFF), Size (Old only), Number, Fade, Frame (the other three styles) |
| `Listtree.mcp` | Right / Down images, Lines / Shadow pens, Style (Normal, Inserted, Lines, Win95, Mac, Lines3D), Space (0-16), Remember status. Left out: MUI 3.8's sample tree (the tree reads its settings in Setup) and "Display opened" (item 0x15, meaning unknown; an item that is there is kept) |
| `Busy.mcc` | reads the eight items; `MUIA_Busy_Speed` is honoured in OM_NEW (it was always 50 ms), `MUIV_Busy_Speed_User` = the setting, 251 = still; the three new styles are our own drawing (what MUI 3.8 draws for Number / Fade / Frame is not known) |
| `Listtree.mcc` | gives Right / Down / Lines / Shadow / Space / Remember / Style (as line type) to its NListtree; without items the MUI 3.8 default (Win95: dotted lines, space 6) |

Seen on `a1200-31` (FS-UAE, 8 MB fast): both pages open, fit 640x256 (`PREFS_SIZES`
build: Listtree 247 x 78, Busy 359 x 118, room 446 x 195; first draft of Listtree 504
wide because of a long credit line, shortened), show MUI 3.8's saved file
(Mac / space 0 / no remember, Old / size 20 / speed 1 ms) correctly, and
`tests/lean/ltbusy.c` shows the classes honouring it. Screenshots
`~/Documents/zune68-ltbusy/` (pg-listtree.png, pg-busy.png, t2.png defaults, t3.png the
imported file, m38-lt.png / m38-busy.png MUI 3.8's own pages for comparison).
Known: the default "1:17" / "1:23" images are drawn stretched in the Right / Down boxes
(MUI 3.8 shows small arrows); Zune prefs shows Lines / Shadow / Right / Down enabled in
every style (MUI 3.8 enables by style); Save writes only items that differ from the
defaults (MUI 3.8 writes all).

## List made smaller (2026-11-11, phase 3)

Branch `listslim` (from `lean`). List was the largest class (`classes/list.o` 28,824 bytes of code).
Measured per function (`m68k-amigaos-nm --size-sort` on the object, `-fstack-usage` against `lean`
after every step), then: what only lists of programs that get MUI 4/5 use went into the list module
(`Zlist.mui`, `listx.c`), and code paths that were there twice became one. Same rules as the rounds
before: the library keeps what layout and the common path need and the decision whether a feature is
used; a MUI 3 program never loads the module for it; no stack frame larger than `lean`'s.

| commit | what | List's code |
|---|---|---|
| 499ea190 | into Zlist.mui: MUI 3.9's horizontal scroller and title buttons (a list has them only in a program that gets MUI 4/5, or asks with `MUIA_List_HScrollerVisibility` Always: `MUIM_Setup`, `List_TitleStrip()`), ISOBJECT cells and `MUIA_List_ColumnOrder` (MUI 5); one function changes the drag event mask | 28,824 -> 26,556 |
| d34ca31f | the dispatcher's cases only pick the method, one call for all; the title's and the entries' column bars one function; `struct ColumnInfo` 64 bytes instead of 66 (its flags as UBYTE), so `data->ci[col]` is a shift at ~110 places | -> 25,840 |
| 6ea458b0 | `MUIM_List_Sort` and `MUIM_List_SortEntries` compared through two copies of one function: now one (`zune_list_compare`); SortEntries (MUI 5) into the module | -> 25,516 |
| d04adeaf, 8daad8bf | `MUIA_List_HideColumn`/`ShowColumn`/`TitleArray` and the format's FIXWIDTH, ISOBJECT, ELIDE, HIDDEN, SORTABLE, ORDER (none of them in MUI 3.8) into the module, called only when a program passes one | -> 25,028 |
| 73e26528, 35201178 | the horizontal check's early outs (quiet, not set up) stay in the library, so a quiet insert never calls the module; `data->area` and the event's message read once | -> 24,984 |
| e8259a6d, 898ca289 | the scrollbar made in one place and its three notifications in a loop; the 15 attributes OM_NEW and OM_SET both stored in one function (OM_SET reaches it through explicit case labels: a foreign tag calls nothing) | -> 24,676 |
| 320680bb, 0182e888 | a cell's four text calls (ELIDE, laid out format, MUI 3.8's cut, plain) one call after working out where the text ends; one `MUIM_List_Jump` call for the keys | -> 24,564 |
| e2a66931 | **found by stackrun**: the bar function made the entry area's clip itself, one frame deeper (Virtual's deepest stack 2,676 -> 2,704 on the first build); now it returns "needs the clip" and `MUIM_Draw` makes it and draws the bars again (the same pixels) | -> 24,636 |

**List's code 28,824 -> 24,636 bytes (-4,188, -14.5%)**; Zlist.mui 4,220 -> 7,820 bytes. Library file
(stripped) 267,716 -> 263,480 bytes (-4,236).

Not reached: the 25-35% aimed at. What is left is code every MUI 3 program runs (per function, bytes:
Draw 2,546, OM_SET 1,946, HandleEvent 1,878, the format parser 1,494, OM_NEW 1,326, DrawEntry 940, Insert
882, dispatcher 902). Tried and taken back or left: GCC already merges identical tails (four inline "remove
the event handler, change the mask, add it" blocks as one function saved 20 bytes); a shared function for
`MUIA_List_Active`'s special values made OM_SET's frame 4 bytes larger for 16 bytes of code, and loops over
the three highlight images in MUIM_Setup/Cleanup 4 bytes each (both taken back); an OM_GET table would be
smaller but slower than GCC's compare tree on every get (not done); the StringArray display/compare hooks
(MUI 4/5) stay in the library because a module call per row costs a class lookup (`zune_module_get`); a
format parser without ReadArgs might save ~1 KB but has to match ReadArgs' rules exactly (not tried).

**Programs that get MUI 4/5 now load Zlist.mui** with their first list (every such list has the
horizontal scroller, laid out by the module): MUI 5's demos and PassPocket in the suite. MUI 3 programs
never: LoadSeg trace below.

Checked (FS-UAE, emulator figures): `make check` after every commit; `-fstack-usage` of every List function against `lean`: none larger (dispatcher -16, DrawEntry -24, LayoutFunction -24, Draw -4); stackrun on the final build: MUI-Demo 5,404 -> 5,368, Layout 2,268, Virtual 2,676, Pages 2,240, Slidorama 2,304 (as `lean`); perfbench (68EC020, 15 reps, `lean` and the branch side by side, then swapped): list insert +0.6 / -0.5%, list active +1.8 / -0.2%, all batches within the spread of two instances. MUI 3 group (on 6ea458b0+, the module moves): 35 of 36 as `lean`'s spread (SimpleMail reset at quit as always), the LoadSeg trace shows no Zlist.mui in any MUI 3 program, only `lean`'s Zrequest/Zpicture loads; AmIRC's tree now has dotted lines, which is `lean`'s own Listtree default (ltbusy), not this branch. MUI 4/5 group (first 12 of 24): identical to `lean`'s except the clock demo's time; YAM 2.10 without a trace: no error window, -392 bytes as alone. Not finished at hand-back: the final build's suite runs, fullcheck and demopix.

**YAM 2.10's "Error messages" window** (smaller2's MUI 4/5 group runs): the LoadSeg trace attached to those runs, not Zune. OS 3.1 has no PNG datatype, YAM tries its 50 theme pictures through datatypes, `wasabi snoop --output full` traces every DOS call and YAM then answers nothing for minutes (its close gadget and Close button do nothing, it does not quit). yam210 alone with a snoop: the window three times of three; without: never. The suite's dismiss hook now ends after two quiet checks; snooplog.sh and real-programs.md say not to trace YAM 2.10.


## Window, Group and String made smaller (2026-11-11, phase 3)

Branch `slim2` (from `lean`). The List round's method on the next three classes: measured per
function (`m68k-amigaos-nm --size-sort` on each object, `-fstack-usage` of every function against
`lean` after every step), then code paths that were there twice became one, and what only programs
that get MUI 4/5 use went into the MUI 4/5 module (`Zmui45.mui`, `mui45.c`). Same rules: the
library keeps the decision whether a feature is used; a MUI 3 program never loads the module for it;
no stack frame larger than `lean`'s. Six tries made a frame 4 bytes larger and were taken back or
narrowed (below).

| commit | what | code |
|---|---|---|
| 7ac59d21 | Window: the dispatcher's cases pick the method, one call for all | 25,972 -> 25,804 |
| 59118ea1 | Window: the four border arrow buttons and the two border props made by one function each (gadget IDs in the same order); the zoomed size and position (width/height, left/top) one function each | -> 25,480 |
| 19af14d4 | Window: the active object's, its parents' and the default object's key handler loops one function (`KeyToObject`); the case-sensitive menu shortcut a function of its own (its copy of the item and its guard left the frame every key passes); MatchIX gets the input event made for MUIA_Window_InputEvent. HandleRawkey's frame 200 -> 160 | -> 25,424 |
| 629106fa | Window: OM_NEW's 13 flag attributes from a table, they and its plain stores one function from its default label (OM_SET keeps its cases: through the function its frame grew by 4) | -> 25,176 |
| c8b7f15b | Window: the four border arrow buttons' clicks one path | -> 25,044 |
| daa66774 | Window: the native menus made in one place (DisplayWindow, UpdateMenu); ToFront, ToBack, ScreenToFront, ScreenToBack one method | -> 24,880 |
| 17d72d42, b125fb8f | Window: the six border gadgets and the five system images freed in loops; the screen released in one place | -> 24,532 |
| a7260d90 | Window: Next and Prev active object one walk with a direction (two iterator functions through a pointer before) | -> 24,444 |
| 5c966035, c8410078 | Window: the contents' limits through one function (WindowResize keeps its call: +4 frame through it); `DeinstallBackbuffer` gone: the buffer bitmap's allocation was never enabled (`#if 0`), so it never had anything to free | -> 24,368 |
| fe34cce5 | Group: the dispatcher picks the method; its frame, on every nesting level's draw, layout and event path, 64 -> 40 | 21,552 -> 21,464 |
| 16aa7319 | Group: `group_layout_horiz` and `group_layout_vert` were mirror copies: one function along either axis (a horizontal group without weights keeps its zero remainder). Its 1D helpers stay out of line and the direction is read from the flags where used, so Group__MUIM_Layout's frame stays 128 (the first build had +36, then +8) | -> 21,140 |
| 9229e5a4, 4241f8a5 | Group: `group_minmax_horiz`/`_vert` as one function along either axis (keeping their three measured differences), -264 bytes. **Taken back (fe7ed791)**: perfbench's relayout was 0.7% slower with an index per child, 0.47% with pointers per axis (side by side with `lean`, whose own figure varied by 0.05%); builds of the branch's steps showed it was this merge (below) | (21,140 kept) |
| c602438e | Group: one constraint and one redistribution call per child (each was there once per axis); the helpers' frames 108 -> 64 and 84 -> 60 | -> 21,000 |
| b6fc0a0f | String: a Multiline String (MUIA_Textinput_Multiline, MUI 4/5's Textinput API) drawn, scrolled to its cursor and hit by the pointer in Zmui45.mui, and MUI 5's placeholder (MUIA_String_Placeholder, V21) drawn there; MUI 3.8 has neither. The library keeps the decision, the marked range and the multiline keys. Draw's frame 140 -> 104 | 19,340 -> 17,720 |
| 28a6d74a | String: the dispatcher picks the method; `String_Textinput` (the Textinput methods every key passes through) out of line: the dispatcher's frame, on every String method, 104 -> 20 | -> 17,656 |
| 32863c27 | String: the SGWork, StringInfo, Gadget and GadgetInfo an edit hook gets are made in a function of their own: HandleEvent's frame, which every key and click passes, 344 -> 144; the list keys from a table | -> 17,644 |
| b6dc0b4f | String: its event handler's events changed through one function (GoActive, GoInactive, MOUSEMOVE on/off); their frames 60 -> 40 | -> 17,580 |

**Window 25,972 -> 24,368 bytes of code (-1,604, -6.2%), Group 21,552 -> 21,000 (-552, -2.6%),
String 19,340 -> 17,580 (-1,760, -9.1%)**; Zmui45.mui 2,376 -> 4,568 bytes. Library file (stripped)
263,208 -> 259,096 bytes (-4,112).

Tried and taken back (each made a frame 4 bytes larger or the code larger): Group's and String's
OM_NEW/OM_SET plain attributes as one function (as List's; +4 on both frames, and String's without
OM_SET was 4 bytes larger, not smaller); Window's MUIA_Window_Width/Height requests as one function
(+4 on WindowSelectDimensions); WindowResize's limits through the shared function (+4); leaving the
never-allocated buffer bitmap out of the opening too (+4 on OM_SET); String's eight cursor methods
from a table (+44: GCC's jump table was smaller). Moving a call that goes deep into a helper puts
the helper's frame on the deepest path: Group's merged layout was checked for that (`-fstack-usage`
of Layout, which nests with the groups). And Group's merged MinMax, smaller and with no frame larger,
cost relayout time (above): an axis picked per child costs more than two copies of the loop.

Not reached: what is left is code every program runs (Window: OM_SET 4.2 KB with the opening
inlined, DisplayWindow 2.5 KB, HandleWindowEvent 1.4 KB, HandleRawkey 1.1 KB, the dispatcher's
compare tree 0.7 KB; Group: AskMinMax 3.3 KB and Layout 2.9 KB with the 2D and page layouts,
group_draw_frame_and_gaps 1.4 KB, Draw 1.3 KB; String: HandleEvent 3.0 KB, Textinput 2.3 KB, Draw
1.6 KB, OM_SET/OM_NEW/OM_GET 1.1-1.4 KB each). A dispatcher table with a binary search would save
~300 bytes per class (estimated) but is slower than GCC's compare tree on every method (not done). String's key
input goes through MUI 5's Textinput methods for every program (`Ti_Call`); a direct path for MUI 3
programs would be faster and smaller but is a rewrite of the editing (not tried).

**Programs that get MUI 4/5 load Zmui45.mui** when they draw a Multiline String or a String with a
placeholder (or set the module's attributes, as before). MUI 3 programs never: LoadSeg trace below.

Checked on the final build (fe7ed791; FS-UAE, emulator figures, `lean` measured in the same session):

- `make check` after every commit.
- `-fstack-usage` of every library function against `lean`: none larger; smaller: String's
  HandleEvent 344 -> 144, its dispatcher 104 -> 20, Draw 140 -> 104, GoActive/GoInactive 60 -> 40,
  Window's HandleRawkey 200 -> 160, DisplayWindow 196 -> 184, UpdateMenu 40 -> 28, WindowClose 52 ->
  48, Group's dispatcher 64 -> 40, its 1D/2D constraint helpers 108 -> 64 and 84 -> 60.
- stackrun (twice each, side by side, then swapped): MUI-Demo with its eight sub-windows 5,368 ->
  5,248, Virtual 2,676/2,400 -> 2,388/2,388 (`lean`'s varies between the two), Layout 2,268, Pages
  2,240, Slidorama 2,304 as `lean`. (On the build with the merged MinMax Layout once showed 2,312,
  then 2,268 in four more runs.)
- perfbench (68EC020, 15 repetitions, side by side, then swapped): create+dispose -3.9 / +0.2%,
  open+close -2.6 / -0.6%, redraw -0.3 / -0.3%, relayout -0.1 / -0.0%, list insert -1.7 / -0.7%,
  list active -3.6 / +0.2%, notify +0.1 / -0.2%, text set +0.2 / -0.1%: no batch slower beyond
  0.2%. (With the merged MinMax relayout was +0.8 / +0.6%, then +0.47 / +0.48%: taken back, above.
  Builds of the steps, five instances at once: Window's commits alone relayout as `lean`, the Group
  commits made it slower, and reversing the dispatcher, the layout merge or the constraint merge
  alone did not bring it back.)
- demopix: 18 of 27 pixel-identical with MUI 3.8 as before; every Zune window pixel-identical to
  `lean`'s, with the same sizes and positions, except Layout (it places its buttons at random).
- fullcheck: MUI-Demo with eight sub-windows and the 18 other demos open and close, the same lost
  figures as `lean` (608 / 200 / AppWindow 264), no alert; LoadSeg: 15 class files (Balance,
  Gauge, Scale, Register, Scrollgroup ...), no module (Z*.mui).
- MUI 3 group (36): all 36 behave as in the reference run (`listslim`'s last, whose Window, Group and
  String are `lean`'s), windows and sizes identical, SimpleMail windowless as always; memory in use on the
  second run identical (to 8 bytes), on the first run from boot 4.5 KB less (the smaller library).
  LoadSeg trace through all 36: only `lean`'s modules where `lean` loads them (Zpicture.mui in the
  NList and TextEditor demos, Zrequest.mui in MUIMine, NewsCoaster, RO, Archiver), no Zmui45.mui.
- MUI 4/5 group (24): the 21 programs of the reference run behave as there (mqttexplorer and
  mui5-embeddedimages exit as always), the three added since (Ticklish, the TabGroup demo, LumiZip)
  run; memory in use on the second run identical (to 48 bytes). YAM 2.10 showed its "Error
  messages" window (its theme PNGs through datatypes) in these runs without a LoadSeg trace too,
  with seven or eight instances running: load, not only the trace, brings it.
- The MUI 5 probes of `make check` run on the emulator (`mui5-*-probe`: string, string_input with
  the edit hook, textinput, marking, selection, layout, pointer, title, text, classes, data, menu,
  menu_input, panel): the same output as `lean` (data and string differ by object addresses only;
  the panel probe fails or passes depending on the load, on both, and passed on both run alone).
- `tests/lean/stringml.c` (three Multiline Strings, one marked, one active, and a placeholder):
  the window pixel-identical to `lean`'s, before and after a click into it.
- Memory, MUI-Demo cold, OS 3.1 + 4 MB fast (`m.sh`, two runs each, side by side): first program
  470,264 -> 466,368 bytes (-3,896, the smaller library), each further 204,512-204,576 as `lean`,
  first window 1.46-1.61 s against 1.54-1.60 s (eight instances running: within the noise).


## Toward the core: what the compiler and the link can still give (2026-11-11, phase 3)

Branch `core` (from `lean`). The three rounds before took the rarely used code out and rewrote the
four largest classes smaller; the file was 259,096 bytes against MUI 3.8's 166,412. This round
looked at the gap structurally before touching code: where the bytes of the stripped library are,
and what every compiler- and link-level lever gives, measured, not assumed. The result is small
(file 259,096 -> 257,712 bytes, -1,384) and the measurements say why the gap will not close that
way: it is the C code of the core classes.

**Where the file's bytes are** (68000 build, GCC 16.2, `hunktool info`, `objdump -r`): code hunk
242,428 bytes (of which ~9.2 KB are strings and ~2.1 KB pointer tables inside `.text`; this target
has no separate rodata), data 364, BSS 3,040, and 4,030 relocations at 4 bytes each = 16.1 KB:
2,272 into the code (1,178 `jsr` to functions in other files, 203 `lea` of functions, 101 `jmp`,
~700 addresses in tables and strings), 1,736 into BSS and 22 into data (1,562 of them `movea.l
_SysBase/_GfxBase/_IntuitionBase/...,a6`: 460 SysBase, 450 GfxBase, 288 IntuitionBase, 121
UtilityBase, 78 DOSBase). The AROS compatibility layer costs nothing: `AROS_LH*`, `AROS_UFH*` and
`BOOPSI_DISPATCHER` are plain function declarations with register arguments (`support_amigaos.h`),
`DoMethod`/`DoSuperMethodA` are libamiga's, `D()` is compiled out, lazylib is one compare per call.

**Measured and not taken:**

- **`-fbaserel`** (data through a4, as MUI 3.8's SAS/C build does). The 1,736 base and global
  loads lose their relocation and 2 bytes each: file 259,096 -> 253,588 (-5.5 KB, measured through a
  wrapper that turns the `lea _f,a0 / jsr (a0)` call sequence `-fbaserel` implies back into
  `jsr _f`, since `-fbaserel` sets PIC and `-fno-pic` cancels it). But the code hunk grows 242,428 ->
  244,204 (+1.8 KB): a4 is no longer a register, so functions under register pressure reload from
  the stack (Area's `Draw_handle_frame` 1,384 -> 1,604 bytes). On top of that every entry from
  outside (45 dispatchers, the hooks, the gates, the process entry) would need `__saveds`
  (`move.l a4,-(sp); jsr ___restore_a4; ...; move.l (sp)+,a4`: 14 bytes and ~60 cycles each, on
  every method of every dispatch chain). More memory, slower dispatch, less file: not taken.
- **`--relax`** (jsr to bsr by the linker): the m68k-amigaos linker changes nothing (byte-identical).
  `-mpcrel` needs 68020 for 32-bit displacements; on a 68000 pc-relative reaches 32 KB, the library
  is 242.
- **LTO.** Rebuilt properly: the library's objects are compiled with `-flto` and linked once with
  `-r -flinker-output=rel` into `core-lto.o`; the final link is an ordinary one, so the hunk
  symbols stay right (the earlier LTO=1 did the LTO at the final link, which the linker writes wrong
  symbols for, and it did not link at all once the module stubs were assembly: LTO made
  `zune_module_get/put` local; they are `externally_visible` now, as `builtinDraw` was). With
  `-fno-inline-functions-called-once` (the stack rule): **252,288 bytes (-6,808)**; with that
  inlining allowed 250,904 (and whole helpers inside the dispatchers); an LTO final link
  (localizing every function nothing outside references) only 536 bytes less than the relocatable
  one, so the -14 KB measured in the first round was the called-once inlining. Partitions make no
  difference (`-flto-partition=one`/`none`/balanced byte-identical). Two things stop it:
  the dispatchers' frames grow even so (estimated from the prologues of the symbol builds,
  `tools`: Notify's dispatcher 4 -> 36 bytes, Poplist 16 -> 68, Window 8 -> 24, Prop/Popobject
  4 -> 32; with `--param inline-unit-growth=0 --param max-inline-insns-auto=0` only Cycle +12,
  Scrollbar/Semaphore +8 and `builtinDraw` +12 remain, for 253,244 bytes), and **`make check`
  cannot run on an LTO build**: the vamos checks (`tests/check_*.py`) trap library functions by
  name (`_zune_window_message`, `zune_imspec_setup`, `zune_font_get`, `Group__MUIM_Layout`,
  `String__MUIM_ClearSelected`, ~60 of them) and LTO inlines some of them into their callers, so
  `check_native_init.py`'s `L_InitLib` runs past its traps into the fake bases (PC in unmapped
  memory after 200,000 cycles). It stays `LTO=1` off by default; the makefile keeps the working
  relocatable-link path and says why.
- **Compiler switches** (48 tried on the whole library, `/tmp/core/s/flagsweep.sh`, stripped file):
  none beyond 0.4 KB without a speed cost. Smaller: `-fipa-pta` -380 (taken, below),
  `-fno-tree-sra` -224, `-fno-ipa-cp-clone -fno-ipa-sra` -224, `-fno-if-conversion` -292,
  `-fno-optimize-sibling-calls` -292, `-fno-tree-loop-im` -216, `-fno-caller-saves` -92 (all
  of these take away an optimization that is also speed: not taken); larger: `-fno-inline` +5.0
  KB, `-fno-guess-branch-probability` +2.3 KB, `-fno-gcse` +1.7 KB, `-fno-ivopts` +1.6 KB,
  `-fno-jump-tables` +1.3 KB, `-fno-inline-functions-called-once` +1.4 KB, `-fno-ipa-icf` +540.
  Nothing: `-fomit-frame-pointer`, `-fno-align-*`, `-fno-tree-loop-distribute-patterns`,
  `-fmerge-all-constants`, `-fno-shrink-wrap`, `-freorder-blocks-algorithm=simple`,
  `-fno-inline-small-functions` (-28). Not in this GCC: `-mregparm`, `-mfastcall`,
  `-mshort-branch`; `-mrtd` is an internal compiler error. `-fconserve-stack` +92.
- **Dead code**: `-ffunction-sections` + `--gc-sections --print-gc-sections` on the library link
  (class table and gates linked, so what the class files and the LVOs reach stays) names 1,644
  bytes nothing calls; 1,060 of them removed (below), the rest are empty gate stubs and
  4-24-byte method stubs the dispatchers reach through tables.
- **Virtgroup as a file** (MUI 3.8 ships Virtgroup.mui): 492 bytes of code against a file's
  ~1.4 KB overhead, and every Scrollgroup loads it. Left in, as decided in class-split.md.

**Taken** (two commits, each building and passing `make check`):

| commit | what | library file |
|---|---|---|
| 097ef113 | Prop's three tiled-image helpers (called only from its AROS-only display hook, `#ifdef __AROS__` now covers them too), the two gradient `to_string` functions (only the debug `zune_imspec_to_string` under MYDEBUG used them), `ZuneLayoutObject`, `ZuneRedraw`, `zune_penspec_draw` (no caller anywhere: library, class files, modules, prefs, tools, checks) | 259,096 -> 258,036 |
| 6a8504fa | `-fipa-pta` in LIB_CFLAGS (whole-unit points-to analysis: fewer reloads, no code shape change; textengine -240, List -196); LTO=1 as the relocatable link above, off | -> 257,712 |

Per object (code bytes, `lean` -> `core`): prop 4,648 -> 3,924, textengine 10,544 -> 10,304, List
24,388 -> 24,192, mui_layoutobj 656 -> 448, penspec 1,248 -> 1,208, mui_redraw 1,404 -> 1,392,
Group 21,000 -> 21,052 (+52, ipa-pta's choice), String +4; Window, Area, Application and the rest
unchanged. Code hunk 242,428 -> 241,064 (-1,364). Class files: unchanged within 50 bytes.

**What this says about the 91 KB left.** Compiler and link together have at most ~8 KB
(LTO 6.8 + ipa-pta + dead code) and the LTO part fights the stack rule and the check harness.
Everything else is the C of the eleven core classes and the engines, all of it on MUI 3
programs' normal paths (List 24.2 KB, Window 24.4, Group 21.1, String 17.6, Area 16.6,
Application 16.0, text engine 10.3, vector images 8.2, frames 6.8, Configdata 6.6, Menuitem 5.0,
Cycle 4.8, Prop 3.9, Text 4.3). Toward 166 KB that is a 35% cut across them: the List round
reached 15%, Window/Group/String 3-9% with the same method. The next wins, in order of size and
certainty: (1) LTO with the check harness taught to run on it (mark the ~60 trapped functions
`noinline, noclone, used, externally_visible` through one header list generated from the checks,
and take the inline-growth params so the dispatchers' frames stay; -5.5 to -6.8 KB); (2) the
string gadget's direct key path for MUI 3 programs (every key goes through MUI 5's Textinput
methods, 2.3 KB; a rewrite of the editing); (3) Area (16.6 KB) and Application (16.0 KB) with the
List round's method (per-function sizes, duplicated paths, MUI 4/5-only parts into Zmui45.mui);
(4) the 16 KB of relocations only go with `-fbaserel`, which costs memory and dispatch time here;
(5) the ARexx command table and help text (~1 KB of strings, Application) into a module if the
trace allows (AmIRC and IBrowse use ARexx at start).

Checked on the final build (4ff62a77; FS-UAE, emulator figures, `lean` built in the same session
from ac762c55 as /tmp/core/base):

- `make check` after every commit (the dead code, the build change, the per-file flag).
- `-fstack-usage` of all 934 library functions, `lean`'s flags against `core`'s
  (`/tmp/core/s/sucmp.sh`): none larger; List's Draw 132 -> 124, zune_text_new 288 -> 272. (The
  first build had `-fipa-pta` everywhere: Group's Layout 128 -> 136; that is the per-file
  exception.)
- stackrun, side by side: MUI-Demo with its eight sub-windows 5,248 bytes, Layout 2,268, Virtual
  2,388, Pages 2,240, Slidorama 2,304 on both.
- perfbench (68EC020, 15 repetitions, side by side, then swapped; this perfbench has no "list
  active" batch): create+dispose +0.8 / -0.7%, open+close -0.2 / +0.2%, redraw +0.1 / -0.2%,
  relayout +0.0 / -0.2%, list insert -0.6 / +0.2%, notify -0.2 / +0.2%, text set +0.1 / -0.1%;
  `lean` against itself on the two instances differed by up to 0.7%: nothing slower beyond that.
- fullcheck: MUI-Demo with eight sub-windows and the 18 other demos open and close, lost 608 /
  200 / AppWindow 264, no alert, as `lean`'s documented figures (the `lean` instance started
  beside it froze in FS-UAE after MUI-Demo with ten instances running and was not rerun).
- demopix: 18 of 27 pixel-identical with MUI 3.8 (the same 18); the 27 Zune windows against
  `lean`'s shots from the same session: 26 identical, Layout differs (its buttons are random).
- MUI 3 group (36, two halves): half 1: 17 of 18 ok, SimpleMail windowless and reset at quit as always in FS-UAE (35 of 36 as every round); half 2: all 18 ok, windows identical to the run
  before the per-file flag.
- MUI 4/5 group (24): 22 of 24 ok, mqttexplorer and mui5-embeddedimages exit as they do every run. (A first run without the plugins built next to the library
  had the MUI 5 demos that need them exit: the suite takes `Libs/MUI` from the build; build
  `core prefs plugins` before `suite.sh`.)
- No module or class file changed hands: nothing moved into or out of `LIBS:Zune/`, so which
  files a MUI 3 program loads is as on `lean` (its LoadSeg trace, the two rounds above); the
  class files are within 50 bytes of `lean`'s.


## LTO, done right, and why it stays off (2026-11-11, phase 3)

Branch `lto` (from `lean`). The round before measured LTO at -5.5 to -6.8 KB and left it off
because the vamos checks could not run on it and the dispatchers' frames grew. This round made
`make check` run on an LTO build, measured every function's frame on it, and found that the
bytes LTO gives *are* the frame growth: LTO stays off. The shipped library (LTO=0) is byte for
byte `lean`'s (257,660 bytes with `LDFLAGS=-s`; `strip --strip-all` gives 258,268 for the same
file, the figure the table below uses for all builds).

**What was wrong before.** The "relocatable LTO link" (`-r -flinker-output=rel`) of the round
before did not compile anything: `rel` means "merge the GIMPLE for a later LTO link" (`nm` on
its output says "plugin needed to handle lto object"), so the real LTO happened in the final
link after all, with the symbol-hunk damage the round had blamed on a final-link LTO.
`nolto-rel` does compile, but it must keep every exported symbol, and then LTO has nothing to
work with: 258,876 bytes, 608 more than `lean`. The library is linked with `-flto` in its final
link now. What that link does to the symbol hunk is harmless and understood: every symbol the
link made local is still listed, all at one address at the end of the code (the plugin's
placeholder), and the names the checks use are kept global, so theirs are right.

**`make check` on an LTO build.** The checks trap and call library functions by name; LTO inlines
a function into its callers or makes it local, and a trap on it is never reached (or the
address is the placeholder). Now:

- `tests/symbols.py` gives every check its symbol table and records each name looked up;
  `tests/trapped-symbols.txt` (315 names: 136 functions, the 85 class descriptors, the
  library bases, and names that live in class files, the prefs program or libnix) is written by
  `make trapped-list` from a run of the checks on an LTO=0 build, and a check that looks up a
  name the list lacks fails and says so, so the list cannot go stale unnoticed. The class walk
  in `check_class_lifetime.py` looks the descriptors up by name for that reason.
- `gcc/trapped.py` turns the list into `$(BUILD)/trapped.h`, included (`-include`) after
  `clib/muimaster_protos.h` in every LTO compile: for each name defined in the library's
  sources a declaration with `noinline, noclone, no_icf, used, externally_visible` (functions)
  or `externally_visible` (data). A declaration without a prototype, `TYPE name();`, needs only
  the return type, read from the definition line (or the AROS_LH*/BOOPSI_DISPATCHER macros'
  arguments), and is compatible with any definition whose parameters are int or wider (C17
  6.7.6.3p15); where a parameter is narrower (BOOL, UWORD, TEXT) or the function is variadic
  the parameter list is copied, the library's own struct typedefs (ZText) written as their tags,
  and each struct tag forward-declared. The module files' `#include "../x.c"` with their
  `#define name zmod_name` renames are followed, so `zmod_load_custom_frame` is found in
  datatypescache.c. Data (`const struct __MUIBuiltinClass _MUI_Area_desc = ...`,
  `struct DosLibrary *DOSBase;`) becomes an `extern` with the tag forward-declared. The
  compiler reports any declaration that does not match its definition: that is how locale.h's
  `VOID Locale_Initialize(VOID)` against the definition's `BOOL` was found (fixed; a plain build
  never sees both in one unit).
- `modules.c` stays a plain object: its ZMC stubs are top-level asm, which an LTO object shows
  to no `nm`, and `gcc/classfiles.py` builds the class files' jump table from the objects'
  symbols (that was the `Zmenu.mui: undefined reference to dt_load_picture`).
- `--param` options are recorded per object at compile time; given only at the link they
  change nothing (`LTO_PARAMS` goes into the compile).

With that, `make check` passes on `LTO=1` (all checks, including the check-builtin build with
every class inside).

**The frames, measured.** `-fstack-usage` does write `.su` files for an LTO link: run the link
from a directory with `-save-temps`, and `muimaster.library.ltransN.ltrans.su` appear there
with every function, local ones too (the round before concluded it did not and estimated from
prologues). Compared by name against `lean`'s `.su` of all objects (clone suffixes stripped,
the largest of a name counts; scripts in `/tmp/lto/s/` this session: `ltosu.sh`, `sucmp.py`):

| build (`strip --strip-all`) | bytes | against `lean` | frames larger than `lean`'s |
|---|---:|---:|---|
| `lean` | 258,268 | | |
| LTO, `-fno-inline-functions-called-once`, trapped list only | 253,516 | -4,752 | 60: Aboutmui's dispatcher 16 -> 136, Poplist 16 -> 124, Listview 84, Slider and Scrollbutton 72, Prop and Popobject 64, Popstring, Notify, Rectangle, Radio, Common 52-60, ten more dispatchers; Text's OM_SET 76 -> 168, area_draw_parent_background 60 -> 104, Group's Layout1D/2D 64 -> 100 / 60 -> 92, Image's Draw +24, zune_imspec_setup/draw +16/+12 |
| + `--param inline-unit-growth=0 --param max-inline-insns-auto=0` (at compile) | 254,616 | -3,652 | the same dispatchers |
| + the methods out of line (`LTO_KEEP`, every `Class__OM_*`/`Class__MUIM_*`) | 257,452 | -816 | 40: Text's OM_SET +92, area_draw_parent_background +44, Layout1D/2D +36/+32, Image's Draw +24, Window's dispatcher +16, ... and 25 of +4 to +8 |
| every function visible, `lean`'s own called-once inlining | 257,080 | -1,188 | 28: Group's Layout 128 -> 136 (every nesting level), MUI_LayoutObj +12, external_load +12, 20 of +4 |
| `nolto-rel` (keeps every symbol) | 258,876 | +608 | |

Why the dispatchers: with LTO a method its dispatcher alone calls is local and called once, and
GCC inlines it whenever that removes the out-of-line copy (a size win at -Os that no
`--param` and not `-fno-inline-functions-called-once` prevents; `optimize("no-inline")` on the
dispatcher does not either). The dispatcher's frame then becomes that method's, and every
other method of the class, and the superclass path, carries it. That inlining is where LTO's
bytes are: with the methods kept out of line 3.9 KB of the 4.7 come back, and the rest of the
growth (+4 in twenty leaf functions, Group's Layout +8) is the cross-file inlining of small
helpers and the IPA register effects, which are the remaining 0.8-1.2 KB. With `LDFLAGS=-s` as
the release builds, the methods-out-of-line LTO library is 257,452 bytes against `lean`'s
257,660: 208 bytes. Under the rules (no frame larger, stack not deeper) there is nothing to
take. The LTO=1 path is kept, documented in the makefile with its knobs (`LTO_ONCE`,
`LTO_PARAMS`, `LTO_KEEP`), so the next measurement starts from a build that checks.

**Checked on the final build** (commits 6e24b55a, 64b2f770; FS-UAE, emulator figures):

- `make check` on LTO=0 and on LTO=1 (every check, both libraries, the check-builtin build).
- The LTO=0 library stripped is byte-identical to `lean`'s (`cmp`), so its speed, stack,
  demopix, suite and fullcheck figures are `lean`'s ("Toward the core" above) by construction.
- The LTO=1 library (methods out of line, stripped, with its class files and plugins), as a
  smoke test of the path, not as a release: stackrun MUI-Demo with its eight sub-windows
  **5,348 bytes against `lean`'s 5,248** (the +4..+92 frames add up on the deepest path: the
  stack rule fails on the real program too), Layout 2,192 (2,268), Virtual 2,340 (2,388), Pages
  2,164 (2,240), Slidorama 2,228 (2,304); fullcheck: MUI-Demo with its eight sub-windows opens
  and closes, the 18 other demos too, lost 608 / 200 / AppWindow 264 as `lean`, no alert: the LTO path works, it just does not pay.

## String's keys edit directly for MUI 3.8; the Textinput methods in Zmui45.mui (2026-11-11, phase 3)

Branch `strkey` (from `lean` at f4534771). The first item "Left open" named after the
compiler and link rounds: String's key input went through MUI 5's Textinput methods for every
program. Each key: HandleEvent -> String_KeyToTextinput -> Ti_Call -> DoMethod -> the dispatcher
-> String_Textinput (2,254 bytes) -> String_HandleVanillakey (1,042) or String_CursorKey (414),
with Ti_Done collecting what to redraw. A MUI 3 program has nothing to catch there (the
MUIM_Textinput_* IDs are MUI 4's), so it paid a method dispatch per key, and the library carried
the MUI 5-only operations (word and line deletes, mark all, toggle case, increment, insert and
append, files, next gadget) for it.

Now (commit 2980f001):

- **One editing function, `String_Edit(cl, obj, data, op, arg)`**: an `SE_*` operation
  (string_private.h: the cursor movements are the MUIKEY values 2-13, then the character,
  Backspace and Shift-Backspace, Del and Shift-Del, copy, cut, paste, and "eaten, nothing")
  on the buffer, the cursor and the mark, answering TIP_EDITED (the contents changed,
  MSDF_CHANGED set) and TIP_UPDATE (a redraw is due). It is HandleVanillakey, CursorKey and the
  tail of String_Textinput folded into one switch with the same results; the caller notifies
  and redraws as before.
- **A program that gets MUI 3.8 calls it straight from HandleEvent** (`String_Key`: the flag
  MSDF_DIRECT59 lines identical on `lean` and the branch (the 45 steps' contents, cursors and notification counts)., set at MUIM_Setup from the application's identity, "Each program gets the
  MUI it asked for" above), and Return acknowledges directly (`String_Acknowledge`) instead of
  through MUIM_Textinput_Acknowledge. The key-to-operation mapping (`String_KeyOp`) is what
  MUI 5's was: Amiga-C/X/V/Z, Ctrl-U/K/X/A/Z, Shift with Backspace and Del, Return as a newline
  in a Multiline String.
- **A program that gets MUI 4/5 has its keys sent as the Textinput methods as before** (`String_Key`
  -> `ti_methods[op]` -> DoMethod with MSDF_INEVENT, so a subclass sees them: the textinput
  probe logs them), and **the methods themselves are in Zmui45.mui** (`zune_string_textinput`,
  mui45.c): what they share with the keys goes back into the library's `String_Edit` through a
  table of the library's editing functions the dispatcher hands over (`struct ZuneStringOps`:
  edit, kill the mark, delete, insert, word bounds, freeze the mark, clipboard write,
  acknowledge); the MUI 5-only ones edit through the same table. The dispatcher reaches the
  module through a three-argument stub (`String_Textinput45`) so its frame stays 20 bytes (the
  fourth argument made it 24). Without the module (not installed, no memory) a Textinput method
  does nothing and answers 0; keys still edit for MUI 3.8 programs, and for MUI 4/5 programs
  the Ti_Call path then edits nothing (the module is loaded with the first key or method).
- The context menu's Cut, Copy and Paste go through `String_Edit` too (Copy leaves the cursor
  where it is now, as Amiga-C does; it jumped to the mark's end).

Found on the way: HandleVanillakey opened the locale for every character typed
(`OpenLocale(NULL)` for `IsPrint`) and closed it only when nothing was added, so each typed
character left the default locale's use count one higher; now ASCII (32-126) needs no locale
and the rest closes it on every path. Amiga-V reported an edit (a notification) even with an
empty clipboard; it reports one when something was pasted or a mark removed (the context
menu's Paste always did).

**String's code 17,580 -> 15,772 bytes (-1,808, -10.3%)**, Zmui45.mui 4,568 -> 6,552 bytes
(stripped). Library file (stripped) 258,268 -> 256,424 bytes (-1,844). Per function (bytes):
HandleEvent 2,920 -> 2,834, String_Edit 1,634 against String_Textinput + HandleVanillakey +
CursorKey + Ti_* 3,970; the module's method 1,282 with its table of 21 methods (126).

Stack: `-fstack-usage` of every library function against `lean`: none larger (ContextChoice 48
-> 44); String_Edit's frame 52, String_Key 24, String_Acknowledge 64; the module's method 72
(its number helper and file reader out of line, 100 and 36). A key in a MUI 3.8 program is now
HandleEvent 144 -> String_Key 24 -> String_Edit 52 against HandleEvent 144 -> Ti_Call 20 ->
DoMethod -> dispatcher 20 -> String_Textinput 104 -> HandleVanillakey 60 before; in a MUI 4/5
program dispatcher 20 -> stub -> modcall 28 -> module 72 -> String_Edit 52 against dispatcher
20 -> String_Textinput 104 -> HandleVanillakey 60: about the same.

**tests/lean/strkeys.c**: the same 45 keystroke steps into nine Strings (plain, Accept, Reject,
Integer, Secret, MaxLen 6, AdvanceOnCR, an edit hook) through input.device on any
muimaster.library, each step's contents, cursor and notification counts printed. Run on `lean`
(the Textinput-method path), this branch and MUI 3.8 at once (a1200-31 twice, a1200-31-mui38):
**`lean` and `strkey` identical in every step**; MUI 3.8 the same except what Zune adds
(Ctrl-cursor moves a word, MUI 3.8 one character; Amiga-C/X/V use the clipboard, MUI 3.8 types
the letter and Amiga-X clears; Alt marks, MUI 3.8 has no marks) -- Accept, Reject, the integer
(742 after Backspace and 7), the secret, MaxLen, AdvanceOnCR (the next String becomes active)
and the edit hook (called 5 times for ab, Backspace, Left, Return) are the same on all three.
Its pointer part does not reach the gadget on either library (input.device takes the events
differently); the mouse marking is tested by the marking probe (below) instead. perfbench has a
"string keys x100" batch now (60 characters and 40 Backspaces as the window hands them to the
active String, each redrawn).

Checked on the final build (2980f001 + docs; FS-UAE, emulator figures, `lean` = the branch
point f4534771 measured in the same session):

- `make check` after the commit (b1 and b2 builds).
- `-fstack-usage` of every library function against `lean`: none larger (above).
- **tests/lean/strkeys.c**: 45 steps (keys; the pointer steps print the unchanged contents on
  every library) identical on `lean` and the branch; MUI 3.8 as described above.
- perfbench (68EC020, 15 repetitions, `lean` and the branch side by side, then swapped):
  **string keys x100 -5.9% / -5.8%** (the direct path); create+dispose -2.3 / +0.5%, open+close
  -0.2 / -1.0%, redraw +0.0 / +0.0%, relayout -0.1 / -0.0%, list insert -1.8 / -0.1%, list
  active -3.2 / -0.4%, notify +0.1 / +0.0%, text set +0.0 / -0.4%: nothing slower beyond the
  spread of two instances (eight instances were running).
- MUI 3 group (36, in two halves with the LoadSeg trace): **36 behave as in the reference run**
  (`slim2`'s final, whose String is `lean`'s; suite-compare: "18 behave the same, 0 differ" for
  each half), windows and sizes identical, SimpleMail windowless as always; memory in use on the
  second run identical except InstallerNG (713,104 against 729,504, its own variation) and
  ViewCSV (+480), on the first run from boot 2.5 KB less everywhere (the smaller library). The
  trace shows **no Zmui45.mui in any MUI 3 program**: only Zpicture.mui (2) and Zrequest.mui (4)
  where `lean` loads them. (The first run of the first half fell over at NList-Demo's menu step
  under the load of twelve instances, a script variable; rerun alone: all 18.)
- MUI 4/5 group (24): **all 24 behave as in the reference run** (suite-compare: "24 behave the same, 0
  differ"; mqttexplorer and mui5-embeddedimages exit as always), memory in use on the second run
  identical to within 24 bytes (AmigaGPT, the gifanim demo).
- fullcheck (with the trace): MUI-Demo with its eight sub-windows and the 18 other demos open
  and close, lost 608 / 200 as `lean`, no alert, the same windows at the same places; modules:
  15 class files (Gauge, Scale, Floattext, Dirlist, Volumelist, Scrollgroup, Register, Poppen,
  Popasl, Pendisplay, Numericbutton, Levelmeter, Knob, Boopsi, Balance), no module (Z*.mui), as
  `lean`.
- demopix: **18 of 27 pixel-identical with MUI 3.8** as before (the same 18); every Zune window
  pixel-identical to `slim2`'s final run except Layout (random button placement): 26 of 27
  screens, MUIDemo-Strings among them.
- The MUI 5 probes on the emulator (a1200-31): string, string_input (13 hook calls), marking,
  selection, data, menu, pointer, text, title PASS; their output identical to `lean`'s run (string and data differ by object addresses only;
  classes and textinput fail the same way on both: Calendar.mcc etc. and Textinput.mcc are not
  installed; menu_input rc=20 on both; panel failed on the branch under the load of twelve
  instances and passed on `lean` alone, as the README notes it depends on the load).
- tests/lean/stringml.c (three Multiline Strings and a placeholder, version 19 = MUI 3.8
  identity, so its multiline keys take the direct path): the window pixel-identical to `lean`'s (on the same session's base build), before
  and after the click into the first one, and the same printed cursor position.
- stackrun (twice each, side by side, then swapped): MUI-Demo with its eight sub-windows 5,248 on both and both times,
  Layout 2,268, Virtual 2,388 (`lean` 2,384 once, as its own variation before), Pages 2,240,
  Slidorama 2,304: as `lean`.


## Area and Application: the dispatcher picks the method, the big buffers off the frames (2026-11-11, phase 3)

Branch `areaapp` (from `lean` at f41e5783). The "Left open" item after String's keys: Area
(16,560 bytes of code, every object's base class) and Application (16,052) still had every
method inlined into the dispatcher (Area's 9,648 bytes with a 132-byte frame, Application's
5,050 with 140), the pattern the List, Window, Group and String rounds replaced with a dispatcher
that picks the method and makes one call. Area's frame is paid on every method of every object,
and at every nesting level of a window's Draw, Layout, AskMinMax, Setup and Show (Group's
methods reach Area's through DoSuperMethod); Application's Load and Save carried a 1 KB file
name each (frames 1,080 and 1,088), its OM_NEW a port name buffer and the commodities broker
(336), its OM_DISPOSE a file name (328).

| commit | what | code |
|---|---|---|
| 95caaab8 | Application: the dispatcher picks the method (frame 140 -> 28); `MUIM_Application_Load` and `Save`, mirror copies, one function with a 280-byte buffer (frames 1,080 / 1,088 -> 344; ENV(ARC):MUI/<base>.cfg, the old ENV(ARC):Zune name still read, the MUI drawer made for Save); the Dataspace's Import/Export on every window through one helper, which CheckRefresh uses too, and `app_windows()` for the eight loops over the windows; OM_NEW's port name and single-task check (`app_make_base`) and its broker (`app_make_broker`) out of line (frame 336 -> 88); OM_DISPOSE's position saving (`app_save_positions`) out of line (328 -> 56) | 16,052 -> 16,044 |
| cf86c5a7 | Area: the dispatcher picks the method; eleven tiny methods (GoActive, DragBegin, CreateShortHelp, FindAreaObject, DeleteBubble, DrawBackground, ContextMenuBuild/Choice, CreateBubble, Relayout, DrawParentBackground) stay inlined, which costs no frame: 132 -> 28. The event handler's events changed through one function (`area_events`: remove, set, add; written out seven times), the repeat timer stopped through one (`area_stop_timer`, three times), the active-object outline's four strips and the drop outline's four edge rectangles in loops | 16,560 -> 17,392 -> 16,964 |
| 556a0ada | Area: the frame title's 3D shadow copy and the outline's four shine copies (eight Move/Text pairs) one loop over a table of offsets, in a function of its own so that `Area_Draw_handle_frame`'s frame stays 176 (inline it was 180); OM_SET's MUIA_ControlChar and MUIA_CycleChain re-registered through one function (`area_rechar`) | -> 16,776 |

| 74ed167d | Area: the hot methods (Draw, AskMinMax, Setup, Cleanup, Show, Hide, HandleEvent) stay inlined in the dispatcher: no extra call on the paths every object takes at every nesting level, and their prologues and INST_DATA loads gone again; the dispatcher's frame 28 -> 84 (what the biggest of them needs; lean's 132). Out of line stay the rare and big ones (OM_NEW/SET/GET, the drag methods, the background drawing, the bubbles, Timer, the frame queries) | -> 16,600 |

**Area 16,560 -> 16,600 bytes of code (+40), Application 16,052 -> 16,044 (-8)**; library
file (stripped) 256,424 -> 256,316 (-108). Zmui45.mui unchanged (6,552).

**Not smaller, and why.** Out of line, Area's 36 methods cost their prologues, epilogues and
their own `INST_DATA` loads: +832 bytes with everything out of line (dispatcher frame 20), won
back in part by the merges above. The tiny methods inlined cost nothing on the frame (28 with
eleven); each further one did: 32 with three frameless methods more, 40 with Import and Export,
48 with OM_DISPOSE, 52 with Hide, Show and QueryFrameCharacteristics. The first full build (the
eleven tiny ones inlined, Area 16,776) showed perfbench's redraw and relayout +0.2-0.4% in both
directions under a load of 17-19, the extra call on every Draw, AskMinMax and Setup of every
object; so the hot methods are inlined too (74ed167d): frame 84, no call, and the bytes back. Tried and taken back: the dispatcher as 16-bit switches on the ID's
lower word after a switch on the upper one (every ID the class answers has 0x8042, 0x9042 or 0
there): GCC's m68k backend widens a switch on a UWORD back to 32-bit compares (`andil #65535`
then `cmpil`), 836 -> 972 bytes. What is left in Area is code every program runs (per function:
OM_SET 1,566, HandleEvent 1,504, OM_NEW 1,350, the background and frame drawing 1,162 and
778, OM_GET 1,108, the dispatcher's compare tree with its eleven inlined methods 1,012). An
OM_GET table would be smaller but slower on every get (not done, as the List round said).

**MUI 4/5-only paths.** Area has none left in the library: its MUI 4/5 attributes
(MUIA_FrameVisible, MUIA_FrameDynamic) have been in Zmui45.mui since `slim2`, and a program that
gets MUI 3.8 skips even the call. Application's non-3.8 parts are MUI 4's
MUIM_Application_Execute/Run (an input loop, 82 bytes) and Zune's own window-position
attributes (which Window and the Configdata use for every program): nothing worth a module
load. So no new module path this round, and the LoadSeg trace below shows, as before, no
Zmui45.mui in any MUI 3 program.

Checked on the final build (74ed167d + the suite script fix 0003043d; FS-UAE, emulator figures, `lean` = the branch point
f41e5783 built in the same session):

- `make check` after every commit (the LTO check build too).
- `-fstack-usage` of every library function against `lean`: none larger; smaller: Application's
  dispatcher 140 -> 28, OM_NEW 336 -> 88, OM_DISPOSE 328 -> 56, Load/Save 1,080/1,088 -> 344
  (one function); Area's dispatcher 132 -> 84 (with Draw, AskMinMax, Setup, Cleanup, Show, Hide,
  HandleEvent and the eleven tiny methods inlined), the others as their own functions now
  (OM_SET 72, OM_GET 44, OM_NEW 40, GoInactive 68, DrawBackgroundBuffered 88, CreateDragImage 184
  as before), area_draw_title 44 under Area_Draw_handle_frame's unchanged 176.
- perfbench (68EC020, 15 repetitions, `lean` and the branch side by side, then swapped, eight
  instances running, load 14-19): **redraw +0.0 / +0.0%, relayout +0.0 / -0.0%**, notify +0.0 /
  -0.0%, open+close -0.3 / -0.9%, create+dispose +1.7 / +0.2%, list insert +0.9 / -0.6%, list
  active +1.6 / -0.1%, text set +0.5 / +0.4%, string keys +0.2 / +0.4%: the batches that move
  move both ways but text set and string keys (+0.4-0.5%, 1,000 ticks of 260,000 over 100
  sets; neither touches what changed: a Text's set redraws through Area's Draw, inlined as
  before). The first full build, with the hot methods out of line, had redraw +0.2 / +0.4% and
  relayout +0.4 / +0.3% in both directions: that was the extra call, and why 74ed167d exists.
- fullcheck (with the LoadSeg trace): MUI-Demo with its eight sub-windows and the 18 other
  demos open and close, the same windows at the same places and sizes, lost 200 / AppWindow 264
  as `lean`, no alert; the result file identical to `strkey`'s final run but for the path of
  the demos; modules: 15 class files (Gauge, Scale, Floattext, Dirlist, Volumelist,
  Scrollgroup, Register, Poppen, Popasl, Pendisplay, Numericbutton, Levelmeter, Knob, Boopsi,
  Balance), no module (Z*.mui), as `lean`.
- demopix: **18 of 27 pixel-identical with MUI 3.8** as before (the same 18); every Zune window
  pixel-identical to `strkey`'s final run except Layout (random button placement) and
  InputHandler (its counter; the compare lists both as not comparable): 25 of 27.
- MUI 3 group (36, in two halves with the LoadSeg trace): **36 behave as in the reference run** (`strkey`'s final, whose Area and Application are `lean`'s; suite-compare: "18 behave the same, 0 differ" for each half), windows and sizes identical, SimpleMail windowless as always; memory in use on the second run identical in every program, on the first run from boot +32 bytes everywhere (InstallerNG 752,912 -> 769,312, its own variation as always). The trace shows
  **no Zmui45.mui in any MUI 3 program**: only Zpicture.mui (2, the NList and TextEditor
  demos) and Zrequest.mui (3: MUIMine, NewsCoaster, RO/Archiver) where `lean` loads them.
- MUI 4/5 group (24): **all 24 behave as in the reference run** (suite-compare: "24 behave the same, 0 differ"; mqttexplorer and mui5-embeddedimages exit as always), memory in use on the second run identical to within 24 bytes (the gifanim demo).
- stackrun (twice each, side by side, then swapped): MUI-Demo with its eight sub-windows **5,248 -> 5,196** (both times), Virtual **2,452 / 2,388 -> 2,212** (`lean`'s own figure varies, 2,384-2,676 in the rounds before; the branch's the same both times), Layout 2,268, Pages 2,240, Slidorama 2,304 as `lean`: the smaller Area and Application frames on the nested paths.
- tests/lean/strkeys.c (45 steps into nine Strings): 59 lines identical on `lean` and the branch (the 45 steps' contents, cursors and notification counts).
- Found on the way: the suite's menu step died on an unbound variable when C:menupos answered
  "failed" (the window gone under load); only numbers are a position now (0003043d).


## Left open

- **Library code**: file 257,660 bytes (2026-11-11, `core`: dead code and -fipa-pta, and the
  measurements of what the compiler and the link can still give, "Toward the core" above;
  LTO measured properly on 2026-11-11, "LTO, done right": its bytes are the methods inlined
  into their dispatchers, which the stack rule forbids, so it stays off and `make check` runs
  on it now; String's direct key path done 2026-11-11, `strkey`: file 256,424, "String's keys
  edit directly" above; Area and Application done 2026-11-11, `areaapp`: file 256,316, "Area
  and Application" above: their dispatchers pick the method, Application's Load/Save, OM_NEW
  and OM_DISPOSE frames 1,080/336/328 -> 344/88/56, Area's 132 -> 84; bytes about even, since
  Area's methods are many and small and the hot ones must stay inlined for speed). Next in
  order: Text and the text engine (9 KB), Notify, Family by the same method, expecting frames
  rather than bytes where the methods are many and small;
  259,096 after `slim2`, 263,208 after List, 267,400 after `smaller2`) against MUI 3.8's 166 KB;
  what is left and the next wins in order: "Toward the core" above.
  Before: 246 KB (file 262 KB, `smaller`), 261 KB (file 279 KB).
  Compiler options do not close it (measured: `-fomit-frame-pointer` -2.7 KB, other `-f`
  switches within 0.3 KB, alignment none, `LTO=1` with bebbo's GCC 6.5
  -3.6 KB and it cannot link the class files: no `nm` plugin, fat LTO
  objects make the linker write nothing). With GCC 16.2, `LTO=1` gives 14 KB but more
  stack use and no `make check` ("A smaller library"). What is left is Zune's own
  code: List 30 KB, Window 29 KB, String 21 KB, Group 20 KB, Area 16 KB,
  Application 15 KB, frames 11 KB, text engine 9 KB. The rarely used
  non-class parts are modules now (above); what is still movable sits
  inside the big classes, on their private data (see "Not moved").
- **First window** 1.72-1.76 s against MUI 3.8's 1.48 s. Loading is not
  the difference: the library opens in 57 ms cold (MUI 3.8: 209 ms with
  muigfx/muilowlevel), a small application and its window take the same
  time as on MUI 3.8; a second MUI-Demo with everything loaded takes 1.47
  s against 1.11 s. Where the rest goes in MUI-Demo is not found yet.
- **Per program**: MUI-Demo's each further 204.5 KB (bytes/1000) against
  MUI 3.8's 210.7-218.8 since phase 4 and its follow-up ("Memory per
  program", "Real programs' memory" above). Real programs' second-run
  figure is 10-80 KB above MUI 3.8's mostly because MUI 3.8 keeps its pool
  puddles after a quit and reuses them; in the first run from boot their
  data is about MUI 3.8's (RO +7 KB, AmigaAMP-Prefs +22 KB, AmFTP -54 KB)
  and their code larger. The first program is 476 against 460 KB: the
  library's code. (Before phase 4: 243 KB against
  211-219; smart refresh backing store 17 KB as MUI 3.8, notifications,
  list pools of 1 KB puddles, Text contents.)

- Tested in FS-UAE only, not on real hardware; RTG only as uaegfx with Picasso96 3.6.3 (rtgfix).
