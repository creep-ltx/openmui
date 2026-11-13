# Roadmap of the lean edition

Decided with our partner on 2026-11-05. The direction: MUI 3.8's look,
memory and speed (better where we can), MUI 3, 4 and 5 programs running.
MUI 3.8 is studied black-box or from its documentation, never from its
code. One phase at a time; each phase ends measured, tested (make check,
fullcheck, the real-program suite) and merged.

Measured on 2026-11-05 (FS-UAE A1200, OS 3.1, 4 MB fast RAM, MUI-Demo):

| | lean | MUI 3.8 |
|---|---|---|
| library code loaded at start | 261 KB (2026-11-06, `smaller`: 246 KB, file 262 KB; 2026-11-11, `core`: 241 KB, file 257,660 bytes) | 166 KB |
| first program | ~497 KB (2026-11-06, `memory`: 479,064 bytes) | 449-460 KB (460,296 bytes) |
| each further program | ~222 KB (2026-11-06, `memory`: 207,704-207,768 bytes) | 206-219 KB (210,696-218,824 bytes) |
| first window | 1.5 s | 1.46-1.62 s |

## Phases, in order

1. **Real-program suite.** More real MUI programs, chosen by what people
   use (mail, web, IRC, FTP, databases, tools), MUI 3.x ones first, then
   MUI 4/5-only ones. Each: install, start, open its main windows, quit;
   screenshots and memory compared with MUI 3.8 (and MUI 5 for 4/5-only
   programs). Runs from one script. Everything after this is checked
   with it. *Built 2026-11-06*: `tests/lean/scripts/suite.sh`, the
   programs, how to run it and the first results (six programs that fail
   on Zune and not on MUI 3.8, eleven more kinds of difference) in
   [tests/lean/real-programs.md](../../tests/lean/real-programs.md).
2. **Demo set compared with MUI 3.8 automatically.** The 19 MUI 3.8
   demos and MUI-Demo's sub-windows, screenshotted on Zune and MUI 3.8
   with the same settings, compared pixel by pixel; a report of every
   difference. The goal is pixel-identical at default settings.
   *Built 2026-11-06* (branch `demopix`): `tests/lean/scripts/demopix.sh`
   with an HTML report; 8 of 27 windows pixel-identical on `lean`, 19
   after the fixes (25 can be compared at all). What is left (group
   distribution, Virtual's border scrollers, the Volumelist's spacing, scaled
   images and the Knob/Levelmeter look, the last two an artwork question) in
   [README.md](README.md#the-demo-set-pixel-by-pixel-against-mui-38-2026-11-06-phase-2).
3. **Smaller library** (target: code loaded at start <= 166 KB). More
   rarely used code into files loaded on demand (window snapshot and
   title gadgets, list editing and drag, string extras, ...), then the
   largest classes made smaller where they do work MUI 3.8 does not.
   Never at the cost of speed or features. *First round 2026-11-06*
   (branch `smaller`): title gadgets, rounded frames, custom frame
   drawing, window and list drag and drop, list editing and the pop-up
   menu strip into modules, one switch made a table: library file
   283,196 -> 268,572 bytes, library alone and first program 13.8 KB
   less, speed and stack as before, no module on a MUI 3 program's
   common path (checked by tracing every LoadSeg through the demos and
   the real-program suite: the rare vector images were tried as a module
   and taken back). 166 KB is not reached: the rest is the core classes'
   own code; what is still movable and what LTO would give in
   [README.md](README.md#a-smaller-library-2026-11-06-phase-3).
   *Second round 2026-11-11* (branch `smaller2`): the module glue as
   10-byte stubs with one routine, the MUI 4/5 attributes of Area and
   String in a module (Zmui45.mui; Area's only for programs that get
   MUI 4/5, as MUI 3.8 ignores them), Configdata's prefs-only setters in
   Zprefs.mui, Crawling a class file: library file 271,676 -> 267,400
   bytes (below the 271,132 before `mui45b`), library alone and first
   program 4.3 KB less, speed and stack as before. The LoadSeg trace
   took three moves back or narrowed them (TheBar, AmTelnet, ViewCSV
   are MUI 3 programs that hit them at start). 166 KB is still not
   reached: the rest is the big classes' own code;
   [README.md](README.md#a-smaller-library-second-round-2026-11-11-phase-3).
   *List 2026-11-11* (branch `listslim`): the largest class rewritten
   smaller, 28,824 -> 24,388 bytes of code (-15.4%; 25-35% was aimed
   at): MUI 3.9/4/5-only parts (horizontal scroller, title buttons,
   ISOBJECT cells, column order and hiding, title arrays, SortEntries,
   the format's MUI 4/5 keywords) into Zlist.mui, duplicated paths made
   one (dispatcher call, column bars, compare, OM_NEW/OM_SET stores,
   text calls), 64-byte column records, an unreachable separator gone.
   Library file 267,716 -> 263,208 bytes; speed, stack and the tests as
   `lean`'s; programs that get MUI 4/5 now load Zlist.mui with their
   first list, MUI 3 programs never;
   [README.md](README.md#list-made-smaller-2026-11-11-phase-3).
   *Window, Group, String 2026-11-11* (branch `slim2`): the same way on the
   next three classes. Window 25,972 -> 24,368 bytes of code (-6.2%),
   Group 21,552 -> 21,000 (-2.6%), String 19,340 -> 17,580 (-9.1%):
   dispatchers that pick the method and make one call, mirror copies made
   one (Group's horizontal/vertical layout by axis (its MinMax too, but
   that cost 0.5% relayout time and was taken back),
   Window's border gadgets, zoom sizes, key handler loops, menus, arrange
   methods, freeing loops), Window's OM_NEW flags from a table, a dead
   buffer path gone; a Multiline String's drawing and MUI 5's placeholder
   into Zmui45.mui (MUI 3.8 has neither). Big rarely used blocks out of hot
   frames: String's HandleEvent frame 344 -> 144, its dispatcher 104 -> 20,
   Group's dispatcher 64 -> 40, Window's HandleRawkey 200 -> 160; no frame
   larger (six tries that grew one by 4 bytes were taken back). Library
   file 263,208 -> 259,096 bytes, first program 3.9 KB less; speed as
   `lean`'s, deepest stack less; MUI 3 programs never load the module;
   [README.md](README.md#window-group-and-string-made-smaller-2026-11-11-phase-3).
   Left: the rest of these classes is code every program runs; Area
   (16.5 KB) and Application (16 KB) not yet looked at this way.
   *Toward the core 2026-11-11* (branch `core`): before more C, every
   compiler- and link-level lever measured on the whole library: where
   the file's bytes are (16 KB of relocations, 1,562 library-base loads),
   `-fbaserel` (file -5.5 KB but code +1.8 KB and `__saveds` on every
   entry: not taken), LTO rebuilt as a relocatable link so the hunk
   symbols stay right (-6.8 KB, but the dispatchers' frames grow and the
   vamos checks, which trap functions by name, cannot run on it: off),
   48 switches (none beyond 0.4 KB without a speed cost), dead code by
   `--gc-sections`. Taken: 1,060 bytes nothing calls, `-fipa-pta` (not
   for Group: its Layout frame). File 259,096 -> 257,660 bytes; no frame
   larger (`-fstack-usage` of all 934 functions), stack, speed and the
   tests as `lean`'s. The remaining 91 KB is the core classes' C; the
   next wins in order in
   [README.md](README.md#toward-the-core-what-the-compiler-and-the-link-can-still-give-2026-11-11-phase-3).
   *LTO, done right 2026-11-11* (branch `lto`): `make check` runs on an
   LTO build now (the names the checks look up are recorded,
   tests/trapped-symbols.txt, and kept out of line and visible through a
   generated header; the -r relocatable LTO link of the round before had
   merged GIMPLE instead of compiling, the library is linked with -flto in
   its final link). Measured with -fstack-usage of every function, LTO's
   bytes are the called-once inlining of the methods into their
   dispatchers (-4.7 KB, dispatcher frames +16 to +120, carried by every
   method of the class); with the methods kept out of line -0.8 KB and 40
   frames larger; with lean's own inlining and every function visible -1.2
   KB and 28 frames larger (Group's Layout +8). Not taken: LTO stays off,
   the shipped library is byte for byte lean's. Details in
   [README.md](README.md#lto-done-right-and-why-it-stays-off-2026-11-11-phase-3).
   *String's keys 2026-11-11* (branch `strkey`): the first of the next wins.
   Every key went through MUI 5's Textinput methods (a method dispatch
   per key) for every program; now the editing is one function,
   String_Edit, which a program that gets MUI 3.8 calls straight from
   HandleEvent (Return acknowledges directly too), while a program that
   gets MUI 4/5 still has its keys sent as the methods and the methods
   themselves live in Zmui45.mui, calling the library's editing back
   through a table. String 17,580 -> 15,772 bytes (-10.3%), library file
   258,268 -> 256,424; no frame larger, a MUI 3.8 key three frames deep
   instead of six; tests/lean/strkeys.c types the same 45 steps into
   nine Strings on `lean`, the branch and MUI 3.8: `lean` and the branch
   identical, MUI 3.8 the same but for Zune's additions (word moves,
   clipboard keys, Alt marking); perfbench's new "string keys" batch
   -5.9% / -5.8% side by side and swapped, the
   other batches as `lean`; stackrun, fullcheck, demopix (18 of 27, every
   Zune window as before), the MUI 3 group (36, no module loaded) and the
   MUI 4/5 group (24) as `lean`. Found: a locale use count leaked per typed character.
   [README.md](README.md#strings-keys-edit-directly-for-mui-38-the-textinput-methods-in-zmui45mui-2026-11-11-phase-3).
   *Area and Application 2026-11-11* (branch `areaapp`): their
   dispatchers pick the method and make one call (frames 140 -> 28 and
   132 -> 84: Area's is paid on every method of every object and at every
   nesting level of a window's Draw, Layout and Setup, and its hot
   methods stay inlined so that they cost no extra call); Application's
   Load and Save one function with a 280-byte name (frames 1,080 / 1,088
   -> 344), its OM_NEW's port name and broker and its OM_DISPOSE's
   position saving out of line (336 -> 88, 328 -> 56); Area's window
   events, repeat timer, control-character registration, focus outlines
   and frame-title copies through one function or loop each. Area
   16,560 -> 16,600 bytes (its methods out of line cost their prologues,
   +832 with all 36 of them for a frame of 20; the merges won most back,
   and the hot and the tiny methods inlined the rest), Application
   16,052 -> 16,044, file 256,424 -> 256,316. No MUI 4/5-only path was left to move (Area's
   went in `slim2`; Application's are MUI 4's Execute/Run, 82 bytes). No
   frame larger; stack, speed and the tests as `lean`. Details and what
   was tried in
   [README.md](README.md#area-and-application-the-dispatcher-picks-the-method-the-big-buffers-off-the-frames-2026-11-11-phase-3).
   Next: Text and the text engine (9 KB), Notify, Family by the same
   method, expecting frames rather than bytes where the methods are
   many and small.
4. **Memory per program** (target: first program <= 460 KB, each
   further <= 210 KB): window backing store, list pools, notifications,
   Text contents. Same rule: never at the cost of speed or features.
   *First round 2026-11-06 (each further program reached, first program not yet)* (branch `memory`): measured first (memtally of every
   task, MUI 3.8 black-box: its second MUI-Demo holds 138.8 KB, Zune's
   144.6), then Area at MUI 3.8's 108 bytes, Group 96 -> 68, List 420 ->
   392, Image and Text smaller, a window's clip stack, buffer rastport and
   custom frames made when it opens, a Text's kept parse in a block of its
   size. MUI-Demo (FS-UAE, bytes): each further 217,240 -> 207,736 (target
   reached, below MUI 3.8's 210,696-218,824), first 488,016 -> 479,064 (not
   reached: the rest is the library's 43 KB of extra code, phase 3; Zune's
   first program on top of the library is 15 KB cheaper than MUI 3.8's).
   Speed and stack as before. Details in
   [README.md](README.md#memory-per-program-2026-11-06-phase-4).
   *Follow-up 2026-11-06* (branch `realmem`): why real programs showed
   10-110 KB more than on MUI 3.8 in the suite. Mostly the measurement:
   MUI 3.8 keeps its pool puddles after a program quits (AmFTP 112 KB, RO
   80, AmigaAMP-Prefs 56) and its second run reuses them; the suite now
   also records the first run from boot. What Zune really spent more: an
   event-node block for every shown object (now only for objects that take
   input, and 80 instead of 120 bytes) and an image spec per object for the
   plain pen backgrounds (now shared). Suite: 32 MUI 3.x programs 285 KB
   less together (RO -34 KB, AmigaAMP-Prefs -36, AmFTP -25), YAM 2.10 -223
   KB; MUI-Demo each further 204.5 KB, first 476.4 KB. First-run data now
   about MUI 3.8's (RO +7 KB, AmigaAMP-Prefs +22, AmFTP -54); the rest is
   code. Speed and stack as before (window opening 3.5% faster). Details in
   [README.md](README.md#real-programs-memory-against-mui-38-2026-11-06-phase-4-follow-up).
5. **Speed** (target: faster than MUI 3.8): start-up, window opening,
   redraw, list scrolling; measured on real 68000/68020 hardware, not
   only in the emulator, and never PiStorm figures as Amiga speeds. The
   real-hardware measurements are done by our partner on a spare A1200
   when one is available; until then emulator figures, labelled as such.
6. **MUI 3 behaviour exactly as MUI 3.8** and **MUI 4/5 programs run**:
   whatever the suite and the demo comparison find, fixed in order of
   how many programs it affects; MUI 4/5 features only as files loaded
   on demand, so MUI 3 programs pay nothing for them.
7. **Settings program as complete as MUI 3.8's**, including reading
   existing MUI 3.8 settings files (their item ids are known for the
   title gadgets; the rest to be measured).
   *First round 2026-11-10* (branch `prefs38`): the inventory (every page
   of MUI 3.8's settings program against Zune prefs and the library) and
   MUI 3.8's file format, measured black-box, in
   [settings-mui38.md](settings-mui38.md). Zune prefs already offered every
   MUI 3.8 setting except Listtree.mcc's and Busy.mcc's pages. The file is
   Zune's own format; ten items have other ids, frame types 5-10 other
   numbers, brushes another prefix, windows their own chunk. Zune prefs
   imports them (menu, or a question on the first run; the MUI files are
   only read). The library now honours the ARexx switch, the drag
   settings and the iconification settings (hotkey, icon, Tools menu item,
   iconified at start). Left: cycle popup speed, middle-button drag, drag
   look, active object colour, the two third-party pages.
   *2026-11-11* (branch `setleft`): the popup speed (MUI 3.8 rolls the
   popup out from the top, measured; Zune through Zmenu.mui) and the active
   object colour (pixel-identical) honoured, +294 bytes. Left: the
   middle-button drag and drag look (not measurable from synthetic input
   in FS-UAE), the Listtree and Busy pages.
8. **Each program gets the MUI it was written for** (decided with our
   partner 2026-11-06, done right after the phase 1 fixes): a MUI 3
   program sees and behaves as MUI 3.8, a MUI 4 program as MUI 4, a MUI 5
   program as MUI 5, with no setting to change. How: exec tells a
   library which version a program asked for when it opens it; the
   shared base reports 21 so every program gets in, and each opener gets
   its own small copy of the base reporting what it asked for (below 20:
   MUI 3.8). Zune's own behaviour follows the same choice per
   application. The "Behave as" setting stays and can override it:
   Automatic (the default), MUI 3.8, MUI 4 or MUI 5, for all programs,
   and per program in a list in Zune prefs. *Built 2026-11-06* (branch
   `identity`): one copy of the base per identity (342 bytes, shared),
   none per program; exec 40/47 pass the version in D2, not D0 as the
   RKRM's example says; classes get the identity of the program they are
   loaded for and are loaded again for another identity once unused.
   Limits: a class in use by one program serves a second program of
   another identity as the first's; most "MUI 4/5" programs ask for 19
   and so get MUI 3.8 under Automatic (only DizzyTorrent of the suite's
   five asks for 20): the per-program list gives them MUI 5. Details,
   measurements and the other limits in
   [README.md](README.md#each-program-gets-the-mui-it-asked-for-2026-11-06-phase-8).

## Phase 6, MUI 4/5 side

- *2026-11-10 (branch `mui45`)*: the MUI 4/5 group widened from 5 to 24 entries (MUI 5's 15 own
  demos that MUI 3.8 lacks, PassPocket, Ticklish, the TabGroup demo, LumiZip), baselined on MUI 5
  and on Zune (Automatic and MUI 5 forced). Added under the partners' rule (MUI 3 programs pay
  nothing): Hyperlink.mcc as a class file, MUI 5's "Z" (zlib) images in the Pixmap class file,
  MUI 5's scrollable contents for a window larger than the screen (in the title gadget module,
  only for programs that get MUI 5), elided texts whole by default, Textdata's quotes; and a
  MUI 3.8 fix that TabGroup.mcc needed (Group adds its initial children itself). The library
  file is smaller than `lean`'s: the settings-only classes Settings and Settingsgroup became class
  files. Details, results and what is left in
  [README.md](README.md#mui-45-programs-a-wider-test-set-and-what-they-missed-2026-11-10-phase-6).
- *2026-11-11 (branch `mui45b`)*: LumiZip's reset at quit found and fixed: it passes a supername
  with garbage as supermcc to MUI_CreateCustomClass; MUI 3.8 and MUI 5 take the name (measured),
  Zune took the garbage and closed it as a library at quit. Added: MUIM_FindObject (MUI 5's
  answers), MUIA_FrameVisible/FrameDynamic (FrameVisible acts, FrameDynamic is kept), List's sort
  mark (in the list module), Title's register tabs (in its class file). UseScreenNotify needs
  nothing (Zune never closes windows for the Workbench). The library grew 544 bytes, about 420 of
  them for MUI 4/5 features MUI 3 programs never call: to be paid back (the Area attributes are
  the candidate). Left: RegisterGadget's popup menu, FrameDynamic's hover. Details in
  [README.md](README.md#lumizips-reset-found-mui-45-calls-programs-use-2026-11-11-phase-6).

## Gate 2, stability (docs/production.md; started 2026-11-12, branches `stab1`, `stab2`)

Tests built as tools and run once each: fault injection (`tests/lean/memtally/failn`,
`failsweep.sh`: every one of MUI-Demo's 2,175 resource calls failed once; no crash of
ours, the children of a failed Window/Application were kept and are disposed now,
`zune_dispose_children`; MUI 3.8 keeps them), the soak (`soak.sh`, 70 min, windows
constant, a creep of about 120 bytes a cycle to attribute), the 68000 (`smallcheck.sh`:
the A600 with 0/1/2 MB fast runs every demo), the parsers (`check_fuzz_parsers.py`,
`check_mui38_fuzz.py`, `prefsfuzz.sh`, `rexxfuzz.sh`: nothing found), `suite.sh --loops K`.
Next in order: the fault sweep over a set of real programs and over MUI-Demo's sub-window
opening (start the count after the main window), the start/quit loops over the 181, the
soak's creep with memtally, the prefs fuzz over more seeds and the per-program files, the
RTG machine, the code audit. Details in
[README.md](README.md#stability-round-1-fault-injection-soak-the-68000-the-parsers-2026-11-12-gate-2).

Round 2 (branch `stab2`): `failreal.sh` swept ten real programs and demos (about 1,000 N's):
two resets of ours fixed (the next flush after a class file failed to load: its shared copy
had no SysBase; a double dispose of a Popstring's or Listview's parts by
`zune_dispose_children`), the library made expungeable again after OS 3.1's MakeClass fails
inside (`zune_make_class`) and after Popasl finds no asl.library; the start/quit loops over
the 181 (705 cycles, no quit crash of ours, growth the same as MUI 3.8's and 0 flushed); the
code audit (six fixes, -Wall clean); RTG by hand (19 of 20 programs on a Picasso96 screen).
Next: the Popup reset seen once, the prefs fuzz over more seeds, the soak's creep with
memtally, the suite subset on the A600, the RTG run through the suite tool on a quiet PC.
Details in [README.md](README.md#stability-round-2-fault-sweeps-over-real-programs-the-startquit-loops-the-code-audit-2026-11-12-gates-2-and-5).

`animfix` (2026-11-12): AnimMaker's resets measured, 25 in 405 boots on lean's library and 9 in
140 on MUI 3.8 (two start/quit runs a boot, Amiberry): the same rate, a write into memory that
is no longer AnimMaker's or its classes', OpenMUI's library life cycle normal in every reset
(decided: AnimMaker's own). The Register page pixel fixed (pagepix equals MUI 3.8's behaviour).
Details in [README.md](README.md#animmakers-resets-and-the-register-page-pixel-2026-11-12-branch-animfix).

## Gate 3, memory and speed (docs/production.md; started 2026-11-12, branches `speed1`, `speed2`, `speed3`)

Round 1: `tests/native/opbench.c` times 27 operations on any muimaster.library (the same binary
on MUI 3.8 and OpenMUI; clicks, typing and the menu as input.device events) in cycle-exact
FS-UAE, `opbench.sh`, `opprof.sh` and `pcsample-full.py` profile one of them. Fixed, biggest
ratio first: List inserts repainting everything (3.26 -> 0.88 of MUI 3.8), a relayout and a
reopen measuring every entry (1.26 -> 0.94, 1.78 -> 0.83), MENUVERIFY in MUI 3 programs' windows
(1.52 -> 1.00), Radio's triple redraw (1.26 -> 0.62), page switches (Cycle 1.88 -> 1.14, Register
1.10 -> 0.99), the scroll clip (1.20 -> 1.07), key matching (1.32 -> 1.20); and a bug: a popup
opened on the default public screen instead of its parent's. 19 of 26 at least as fast as MUI 3.8.
Memory: first program 465.9 against 460.3 KB, the gap is library code. Next: typing (the key
path per event), the Cycle's Hide/layout/Show, the row scroll, object creation (pools?),
pointer.c and further MUI 4/5 code into modules for the first program. Details in
[README.md](README.md#speed-round-1-every-common-operation-timed-against-mui-38-2026-11-12-gate-3).

Round 2 (branch `speed2`): measured by leaving one part out at a time (`-DZEXP`). A key went to
the active object twice and to a parent twice (MUI 3.8 and MUI 5: once each, `keyoffers.c`):
typing 1.19 -> 0.99-1.04 of MUI 3.8 (its own two instances differ by 5%); a page switch laid out
pages that had their box: Cycle 1.14 -> 0.98; the scrollbar sync sent four tags per row: scroll
1.07 -> 0.98, list active 0.91. First program 465.9 -> 460.2 KB against MUI 3.8's 460.3: pointer
shapes, Text shortening and marking, RGB text pens and MUIO_ImageButton into Zmui45. 23 of 26
at least as fast as both MUI 3.8 instances. Left: creation 1.06-1.08 (MUI 3.8 pools: 53
allocations against 150, `alloccount.c`; a pool here cost the first program 3-7 KB). Details in
[README.md](README.md#speed-round-2-typing-cycle-the-row-scroll-creation-and-the-first-program-2026-11-12-gate-3).

Round 3 (branch `speed3`): opbench against real MUI 5 (`opbench v21` on a1200-32-mui5, OpenMUI
in its place with the MUI 5 identity): OpenMUI faster in all 27 operations (0.87 to 0.02). The OS
calls of every operation counted black box (`opbench count`, tests/native/syscount.h: SetFunction
stubs on about 130 functions; `who=NAME` names the library's callers). Fixed what OpenMUI asked
for and MUI 3.8 does not: utility's NextTagItem on every tag walk (now in the library), GetAttr
for an object's own state (the event handlers' disabled check: MUI 3.8 and MUI 5 ask nothing,
tests/native/disabledget.c; Group's AddObject, Menuitem, Area's cleanup, List and Slider), a list
row scroll drawing a row twice with a clip, MapRawKey for key releases, an allocation per slider
value. button click 0.94 -> 0.78 of MUI 3.8, creation 1.06-1.08 -> 0.97-1.01, typing 0.99-1.04 ->
0.92-0.94; 24 of 26 at least as fast as MUI 3.8 (not yet: full redraw and register page against
MUI 3.8's faster boots, 1.06-1.09). Zune68 main measured beside them (slider set 37x MUI 3.8). First
program 459,992 bytes (MUI 3.8 460,296). Left: a window's menu built with about 280 GetAttr per
opening, creation's allocations (MUI 3.8 pools), a disabled object's event handler (MUI calls
it). Details in
[README.md](README.md#speed-round-3-against-mui-5-the-os-calls-per-operation-the-getattr-calls-2026-11-12-gate-3).

Round 4, "prove it" (branch `draw1`, 2026-11-13): targets of 25 % below MUI 3.8 for the window
opening, full redraw, relayout, resize, register page and uniconify, and memory toward 400 KB /
185 KB; measured on the A600's 68000 too (MUI 3.8 copied onto an a600-31 instance). Reached:
none of the 25 % targets (open+close 0.79-0.81, uniconify 0.79-0.80, register page 0.85, popup
0.81, full redraw 0.98-1.06; the resize is the OS's own 218.7 ms). Found and fixed: clip regions
nobody needed (Register titles, page backgrounds, List repaints), the window's background
painted twice at an opening, ScreenToFront's vertical-blank wait at every opening,
InitRastPort per text measured; and, added by the partner, the window border arrows (they
scrolled nothing; now MUI 3.8's 8 pixels and repeat) and a Virtgroup scroll 40x slower than
MUI 3.8 (2.87 s -> 93 ms a step). Memory not reached (first program +2.4 KB of code). What a
25 % redraw needs is the library's own CPU per draw: the Group, Area and text-engine rewrites
below. Details in
[README.md](README.md#prove-it-drawing-and-memory-2026-11-13-branch-draw1).

## Rewrite for m68k (started 2026-11-13, branch `rw-list`)

The partner's decision: "We must rewrite the bloated AROS parts. OpenMUI is for m68k machines,
not x86/64." Class by class, the largest first, each as its own branch with the gates' rules
(the same behaviour and pixels, not slower, memory per program not more, no stack frame larger,
MUI 3 programs pay nothing for MUI 4/5): data structures for a 16/32-bit machine with little
memory, the common case first, AROS portability layers and generality removed, assembly only
where a measured hot loop pays. Order:

1. **List** (done 2026-11-13, `rw-list`): rows as one array of 8-byte entries, one block per
   format, shared hooks, the plain-cell fast path, the dispatcher as a table. Memory per
   MUI-Demo -7 KB, list insert 1000 -48 %, sort -14 %. Details in
   [README.md](README.md#rewrite-list-2026-11-13-branch-rw-list).
2. **Window** (done 2026-11-13, `rw-window`): no memory pool per window (the cycle chain one
   block, the gadget IDs a byte map), the dispatcher as a table, the opening, the pens and the
   menu events out of the hot functions' frames, OM_GET/OM_SET/OM_NEW and the setup rewritten,
   MUI 5's case-sensitive shortcuts and MENUVERIFY and the snapshot's file editing in modules.
   Window code -12.7 %, library -3.1 KB, first program -4.4 KB, each further -1.3 KB, speed
   as lean. Details in [README.md](README.md#rewrite-window-2026-11-13-branch-rw-window).
3. Group (draw1 measured it: the background machinery, the gap sweep and Area's dispatch are
   most of the library's 38-41 % of a full redraw; a full redraw makes the same OS calls as
   MUI 3.8, so a faster redraw has to come from here).
4. Area.
5. Application.
6. String.
7. The text engine (textengine.c).
8. Image and frame drawing (imspec, frames).

## Later (wanted, not scheduled)

1. Hand-written 68000 assembly in the hottest drawing and layout loops.
2. The remaining AROS-derived code rewritten smaller, piece by piece.
3. MUI 5's extra classes and features as optional files.
4. Faster drawing on graphics cards (RTG).

## Bugs found from outside

1. **MUI_AslRequest returns at once without a requester** (found 2026-11-06 with PFS3Doctor on the A1200,
   Zune68 Beta 2, muimaster 21.227 of 2026-11-02): `MUI_AllocAslRequestTags(ASL_FileRequest,
   ASLFR_TitleText, ASLFR_InitialFile, ASLFR_DoSaveMode TRUE)` then `MUI_AslRequestTags(fr, ASLFR_Window, w)`
   showed nothing and saved nothing. Real MUI 3.8 (FS-UAE a1200-31-mui38) opens the requester with the same
   code. Suspect the MUI file panel path (`ZuneAllocAslRequest`/`ZuneAslRequest`, used when every tag is
   supported by the panel). PFS3Doctor now calls asl.library itself.
   *Checked 2026-11-06 (lean 76619647):* reproduced on the real A1200 (OS 3.2, asl.library 47.11, 1280x960
   24-bit RTG Workbench) with a small test (AllocAsl/AslRequest as above): FALSE at once with the MUI 3.8
   identity (plain asl.library path) and the MUI 5 identity (Zune's panel). FS-UAE OS 3.1 and OS 3.2.1 (asl
   47.9, planar 640x256) open both requesters. Plain `AslRequest()` called by the test itself with
   `ASLFR_Window` = a Zune window reset the A1200: Software Failure 8000 0004 (illegal instruction), task
   0x08008FC2, next to input.device's task. Next: reproduce on an emulated RTG (uaegfx) OS 3.2 screen.
   *2026-11-07 (branch `prefsfast`)*: the test is kept as `tests/lean/asltest.c` (`asltest <version> [raw]`);
   the RTG reproduction is not done yet (the session went to item 3 first).
   *Fixed 2026-11-07 (branch `rtgfix`)*: reproduced in FS-UAE on a uaegfx 1280x1024 24-bit screen
   (Picasso96 3.6.3). The cause was the window pointer, not asl: the test and PFS3Doctor passed
   `_window(win)`, an area macro, on a Window object; Zune's Window data made it a pointer into the
   object, MUI 3.8's makes it NULL (measured). A Window now gives NULL there as on MUI 3.8, and the
   requester opens. Details in
   [README.md](README.md#rtg-mui_aslrequest-zune-prefs-on-640x256-2026-11-07).
2. **A List eats every key while the pointer is over it** (found 2026-11-06 with PFS3Doctor, FS-UAE with the
   A1200's muimaster 21.227): List's HandleEvent, IDCMP_RAWKEY, sets `result = MUI_EventHandlerRC_Eat`
   whenever the pointer is over the list or its scroller, outside the switch that only handles the wheel
   codes, so ControlChar buttons stop working after a click into a list. Eat only RAWKEY_NM_WHEEL_UP/DOWN.
   *Fixed 2026-11-06*: only the two wheel codes are eaten. `tests/lean/listkey.c` (a list and a `_Log`
   button): with the pointer on the list, L pressed the button 0 times before, 1 time after (FS-UAE a1200-31).
3. **Zune prefs took 84 s to open its window** (FS-UAE A1200 OS 3.1, 640x256; the partner's A1200 felt slow
   too). *Fixed 2026-11-07* (branch `prefsfast`): 5.7 s. NBitmap's dithering of the MCP icons (55 s),
   Pixmap's bzip2 small mode, all pages made at start, every .mcc opened to look for settings classes, list
   images dithered again on each of the five setups a too-big window gets. Details in
   [README.md](README.md#zune-prefs-opens-in-6-s-instead-of-84-2026-11-07). *2026-11-07 (branch `rtgfix`)*: the
   window fits 640x256 at its normal fonts (System page 472 x 217 -> 407 x 162), opens after one
   setup (4.7 -> 3.3 s); a larger page shown later reopens the window to fit it.
   *2026-11-07 (branch `prefsfit`)*: 16 of 17 pages fit the 640x256 Workbench at topaz 8 (Windows, Groups,
   Listviews, Navigation, TextEditor.mcp, NListviews.mcp and NListtree.mcp rearranged, every setting kept);
   the reopen happens there only for NListviews (still about 20 px too tall); TheBar.mcp's page (8 x 8) shows its settings (Scrollgroup with AutoBars on
   an axis that does not scroll); and the library now does what MUI 3.8 does when a window's contents grow
   past the screen (moved up, or closed and opened again in a smaller stage, or closed), measured black-box.
   Details in [README.md](README.md#every-settings-page-fits-640x256-a-window-that-grows-past-the-screen-2026-11-07).
   *2026-11-07 (branch `diffs`)*: NListviews.mcp's page fits too (428 x 171: the credits crawl through
   one line); Zune prefs stays in topaz 8 on every page. Details in
   [README.md](README.md#the-suites-last-differences-against-mui-38-2026-11-07).

4. **A program lost its identity after another program had used a class library** (found 2026-11-12 in
   compatibility round 4 with `tests/lean/probe/idprobe.c`): a class library cached from a program of one
   identity is dropped (`RemLibrary`) when a program of another identity loads it; its expunge closed
   muimaster.library on the new program's task, which took the program's only opener count, and the
   reload's LibOpen opened a fresh record with the class's own version: the program had the right library
   base but its application behaved as MUI 3.8. *Fixed 2026-11-12* (branch `compat4`): a nesting depth in
   the record around the library's own open/close/remove of class modules, and a hash of the program's name
   in the record (a Shell's `run` gives every command the same task address and stack). Details in
   [README.md](README.md#compatibility-round-4-the-identitys-records-mui-5s-geometry-for-mui-45-programs-the-spare-pixels-the-iconify-setting-2026-11-12).

## Phase 6 progress

- *2026-11-07 (branch `diffs`)*: the suite's last window differences against MUI 3.8 worked through,
  measured black-box: horizontal Radio spacing (TheBar demo2/demo3), the application base name in upper
  case and the window maximum from the real borders (AmTelnet), List/Listtree sizes (AmIRC), the preset
  fonts in the render info where MUI 3.8 keeps them (IBrowse 2.5.9 opens its main window). Left: MUI-Demo's
  Listviews window (2 px), the group spare-pixel distribution, the ARexx port made at once, AmIRC's and
  AmTelnet's logos. Details in
  [README.md](README.md#the-suites-last-differences-against-mui-38-2026-11-07).
- *2026-11-10 (branch `leftovers`)*: three of the four done, measured black-box: the ARexx port made in
  OM_NEW as MUI 3.8 (and commands from any host, not only from ARexx, with MUI 3.8's -3/-4 answers), AmIRC's
  and AmTelnet's logos drawn (Bitmap's and Bodychunk's temporary row had one plane), MUI-Demo's Listviews
  window 292 wide (a List's inner spacing inside its frame, Volumelist's 4 on the left). The group
  spare-pixel rule: a model that fits 1,308 of 1,312 measured rows (`tools/lean/groupfit.py`) was built
  and taken out again, because MUI-Demo's Groups window and the Pages demo then differed; the
  counter-example is measured. Left: that, the Listviews window's column widths and assign image.
  Details in
  [README.md](README.md#the-leftovers-arexx-port-logos-listviews-group-spare-pixels-2026-11-10).
- *2026-11-11 (branch `compat1`, compatibility round 1)*: the suite widened from 60 to 157 entries
  (48 more programs chosen by Aminet's download counts and its 2026 uploads, Voyager 3.5.3 among
  them, and a new group `mcc` with each third-party class's own demo: 49 demos from 35 archives),
  all baselined on real MUI 3.8, real MUI 5 and OpenMUI. New programs: 36 of 48 as on MUI 3.8;
  class demos 37 of 49, 39 after the fix. Fixed: Window's OM_NEW applied its tags through the
  subclass's OM_SET (BWin.mcc's borderless window never opened). Open, ranked: Voyager's reset
  after a close-gadget quit (memory list insane; ARexx QUIT clean), NFS-GUI ending with rc 20
  before its window, the TipOfDay demo's title set after MUIM_Tip_Show, Coloradjust's default
  size (Lamp demo), OpenMUI's own Busy.mcc look, and nine windows within 1-58 pixels. Details in
  [README.md](README.md#compatibility-round-1-48-more-programs-49-class-demos-bwins-window-2026-11-11).
- *2026-11-12 (branch `compat2`, compatibility round 2)*: NFS-GUI 1.1 opens (Register's frame
  spacing in the fitting stages left its window 3 pixels too tall for 640x256), the TipOfDay
  demo's custom title (Window's OM_GET cleared a subclass's stored value), the root object keeps
  its frame (TinyMeterPrefs), the set()-in-OM_NEW audit measured on MUI 3.8 and List, Scale,
  Popobject, Popasl made to match; AmiTradeCenter's Server Manager is the program's (reqtools);
  Voyager's reset traced to a clear into a page bitmap its own quit routine freed, by its own
  task (three memory tools in tests/lean/memtally; the writer not named: open). Details in
  [README.md](README.md#compatibility-round-2-nfs-gui-tipofday-the-om_new-audit-voyagers-quit-2026-11-12).
- *2026-11-12 (branch `compat3`, compatibility round 3)*: round 2's stack frames put back
  (Popobject, Popasl, Scale); Voyager's write named (its own blit, still running when its own
  FreeBitMap frees its double buffer; the buffer is made only on OpenMUI, by Voyager's own layout
  state: decided as Voyager's, a lead kept) with a new black-box method tracer (mccspy);
  Coloradjust, a bare List's scroller, Busy's size and frame, Rectangle bar titles measured on
  MUI 3.8 and matched; 16 programs and 11 class demos more (132 and 60). Details in
  [README.md](README.md#compatibility-round-3-the-frames-repaired-voyagers-write-named-coloradjust-list-busy-27-more-entries-2026-11-12).
- *2026-11-12 (branch `compat4`, compatibility round 4)*: round 3's bar titles, a bare List's scroller,
  Busy's size and the unset Tiny/Title/Big/Knob fonts follow the program's identity (MUI 5 measured with
  tests/lean/probe/bars.c: titles 4 wider in a topaz 6 title font, a List with its own scrollbar, Busy
  18x11, fonts two sizes smaller/larger); the identity records fixed (bug 4 above); Scrollgroup's default
  as MUI 3.8's (100x84 within the contents' sizes); the iconify-at-startup setting applied by Application's
  own OM_SET; Voyager's set shown to be its own (its content List's answer differs); the group spare-pixel
  rule measured further and left; Popscreen's WindowHook measured and left. Details in
  [README.md](README.md#compatibility-round-4-the-identitys-records-mui-5s-geometry-for-mui-45-programs-the-spare-pixels-the-iconify-setting-2026-11-12).
- *2026-11-13 (branch `rename1`)*: OpenMUI's own drawers renamed (`LIBS:OpenMUI`,
  `ENV:OpenMUI`, `ENVARC:OpenMUI`, the catalogs), an earlier installation's settings moved by the
  installer (OpenMUISwitch `MIGRATE`), the user's files in `LIBS:Zune` kept in the backup, the old
  settings read by the library until moved and copied once by OpenMUI prefs; the Aminet pair is
  `OpenMUI.lha`. First program 460,176 bytes (MUI 3.8 460,296), no frame larger. Details in
  [README.md](README.md#openmuis-own-drawers-libsopenmui-envopenmui-envarcopenmui-2026-11-1213-branch-rename1).
