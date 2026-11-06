# Zune68, lean edition

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

(lean measured 2026-11-05 with GCC 16.2, after the modules below; smaller
2026-11-06 (branch `smaller`, [below](#a-smaller-library-2026-11-06-phase-3));
memory 2026-11-06 (branch `memory`, [below](#memory-per-program-2026-11-06-phase-4):
479,064 / 207,736 bytes, MUI 3.8 the same session 460,296 / 210,696-218,824;
this table is in KiB); realmem 2026-11-06 (branch `realmem`,
[below](#real-programs-memory-against-mui-38-2026-11-06-phase-4-follow-up):
476,368 / 204,456-204,504 bytes),
where lean measured the same session gave 289 / 490 / 212 KB; the other
rows 2026-11-04.)

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

## Left open

- **Library code**: 246 KB (file 262 KB, 2026-11-06, `smaller`) against
  MUI 3.8's 166 KB; what is left: "A smaller library" above. Before:
  261 KB (file 279 KB).
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

- Tested in FS-UAE only, not on real hardware; Dtpic not on an RTG screen.
