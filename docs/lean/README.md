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
| lean | 277 KB (+215 KB in 44 class and module files) | 289 KB | 497 KB | 222 KB | 1.5 s |

(lean measured 2026-11-05 with GCC 16.2, after the modules below; the other
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

## Left open

- **Library code**: 261 KB (file 279 KB) against MUI 3.8's 166 KB.
  Compiler options do not close it (measured: `-fomit-frame-pointer` -2.7 KB, other `-f`
  switches within 0.3 KB, alignment none, `LTO=1` with bebbo's GCC 6.5
  -3.6 KB and it cannot link the class files: no `nm` plugin, fat LTO
  objects make the linker write nothing). What is left is Zune's own
  code: List 30 KB, Window 29 KB, String 21 KB, Group 20 KB, Area 16 KB,
  Application 15 KB, frames 11 KB, text engine 9 KB. The rarely used
  non-class parts are modules now (above); what is still movable sits
  inside the big classes, on their private data (see "Not moved").
- **First window** 1.72-1.76 s against MUI 3.8's 1.48 s. Loading is not
  the difference: the library opens in 57 ms cold (MUI 3.8: 209 ms with
  muigfx/muilowlevel), a small application and its window take the same
  time as on MUI 3.8; a second MUI-Demo with everything loaded takes 1.47
  s against 1.11 s. Where the rest goes in MUI-Demo is not found yet.
- **Per program** 243 KB against 211-219 KB: 17 KB is smart refresh
  backing store, the rest is spread (notifications 13 KB, list pools
  14 KB of 1 KB puddles, MUI's own default is 2 KB, Text contents and
  text engine 8.5 KB, Group's virtual-only fields ~4 KB).

- Tested in FS-UAE only, not on real hardware; Dtpic not on an RTG screen.
