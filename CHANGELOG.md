# Changelog

All notable changes to OpenMUI. Betas are numbered `X.YbN` until OpenMUI has
been in daily use for a while. Figures are from emulators (Amiberry, FS-UAE),
not from a real Amiga.

OpenMUI is not MUI and is not affiliated with MUI's authors.

## Unreleased

### Fixed

- **The window border's scroll arrows** (a Scrollgroup with
  MUIA_Scrollgroup_UseWinBorder, as in MUI-Demo's "Virtual" window) scrolled
  nothing; they now scroll 8 pixels a click, and held they repeat after a
  short pause, as in MUI 3.8. The border scrollers' knobs now show the
  position. A Scrollgroup's own arrows also step 8 pixels (they stepped 1).
- **Scrolling a large virtual group** was very slow: an 8-pixel step of a
  group of 80 buttons took 2.9 s on a 68020 (MUI 3.8: 71 ms); now 93 ms.

### Changed

- **Programs start faster, and Application was rewritten for 68k machines.**
  Making a program's objects takes about a fifth less time than before and
  than in MUI 3.8 (opbench's window full of gadgets: 183-189 ms on a 68020
  against MUI 3.8's 236; 758 ms on a 68000 against 929-948), and an
  application with no windows a sixth of MUI 3.8's time (16 ms against 92).
  A program that never stored a window position no longer reads and writes
  its settings file at every quit; the settings are found by hash instead of
  a search through a list (an application with a full settings file was made
  in 321 ms, now 57, on a 68020; 1.37 s, now 0.24, on a 68000), and saved key
  bindings are read once for all programs. What a program reaches only
  through the user or ARexx (ARexx commands, the icon while iconified,
  Exchange's commands, dropped icons, the help, the About and settings
  windows) is a file of its own, `LIBS:OpenMUI/Zapp.mui`, loaded the first
  time it is used: Application's code is 29 % smaller and the library 3.6 KB.
  The first program needs 439.8 KB (443.0 before, MUI 3.8 460.3), each
  further one as before (193.5 KB, MUI 3.8 210.6-218.9).
- **Fixed on the way:** a damaged settings file whose strings lack their
  terminators no longer makes the library read past a setting into other
  memory.
- **Area rewritten for 68k machines**: the class every visible object is
  built on draws its background and frame in one short step in the common
  case (a plain colour, an ordinary frame): one walk up the window to find
  the colour, one fill, the frame, where it used to look the frame up
  twice, walk up twice and first paint the frame's top rows with the
  background only to paint the frame over them. On a 68000 a full window
  redraw now takes 445 ms against MUI 3.8's 470-477 (it was 475-478, level
  with MUI 3.8); on a 68020 134 ms against 141-151 (was 141). Relayouts are
  4-5 % faster, a Cycle or Radio set about 10 %, a button click 5-14 %.
  Rarely used parts load only when used: drag and drop, context menus and
  rounded frames' backgrounds went into the files those features load
  anyway, and AROS's own Area methods into the MUI 4/5 file. The library's
  code is 1.4 KB smaller (Area's own 8.8 %), the first MUI-Demo needs
  443,048 bytes (444,6xx before; MUI 3.8 460,296), an object that reacts to
  input 8 bytes less. Programs get the same attributes, methods and
  pixels. NewsCoaster 1.61 resets the machine when its first-start "New
  User" window is confirmed: its own bug (it writes 8 bytes 32 KB above
  its stack, which on the test machine is the library's code, on the old
  code too; the code that now lies there is run).
- **Group rewritten for 68k machines**: the class every window is built
  from (rows, columns, pages, scrolling areas) lays out, measures and draws
  with less of its own work: one routine for both directions instead of
  two, no function call per child when it walks its children, the gaps
  between children filled with one pen in one go when the background is a
  plain colour, rarely used parts (tab titles, grids, scrolling areas)
  kept apart. On a 68020: a full window redraw 148 -> 141 ms (MUI 3.8
  141-151), a relayout 193 -> 180 (198-207), creating an application with
  its windows 235 -> 219 (235-239), opening a window 420 -> 408, a Cycle
  set 2.38 -> 2.21, a Register page 45.4 -> 43.7. On a 68000: the full redraw 505 -> 478 ms (MUI 3.8 470-478; it was 7 % slower than MUI 3.8, now even), a relayout 693 -> 643 (MUI 3.8 698-706). Each group
  takes 36 bytes instead of 68 (MUI 3.8: 42): the first MUI-Demo needs
  444,592 bytes (was 450,848; MUI 3.8 460,296) and each further one 193.6 KB (196.8). The library's code is 3 KB smaller. Programs see no
  difference: the same attributes, methods and pixels.
- **Window openings, page switches and popups draw less**: a window that has
  just been painted with its background no longer paints the same background
  again behind its groups; Register tabs, page switches and list repaints no
  longer set up clip regions when nothing can spill over (each cost about
  2 ms on a 68020); opening a window on the front screen no longer waits for
  the next display frame. Window open+close 440 -> 420 ms (MUI 3.8 520-536),
  uniconify 370 -> 357 (446-453), a Register page 54 -> 45.5 (54), a popup
  120 -> 97 (120); on a 68000 the window opening 1537 -> 1479 ms (MUI 3.8
  1763-1781) and a Register page 195 -> 158 (161). Pixels as before.
- **List rewritten for 68k machines (first part)**: a list's rows are now one
  compact array (8 bytes a row, no memory block per row), a list format is one
  block, and the common case, a row of plain text, is measured and drawn
  without the text engine's general path. Each MUI-Demo now takes about 7 KB
  less memory (first program 452,848 bytes against MUI 3.8's 460,296);
  inserting 1000 entries takes half the time (248 against 473 ms; MUI 3.8
  6,650 ms), sorting 1000 14 % less (94 ms; MUI 3.8 658 ms), redrawing a
  window 1 % less. The library's code is 840 bytes smaller. Programs see no
  difference: the same attributes, methods, hooks and pixels.
- **Window rewritten for 68k machines**: a window no longer keeps a memory
  pool of its own (it held a 1 KB block for every window that had been
  opened); its Tab order and gadget numbers are now small blocks. The code a
  window runs is 3.1 KB smaller (12.7 %), and the parts only MUI 4/5
  programs use (case-sensitive menu shortcuts, the menu-open check) and the
  saving of a window snapshot to disk now load on demand. Each MUI-Demo
  takes 1.3 KB less memory, the first one 4.4 KB less (448,488 bytes
  against MUI 3.8's 460,296). A MUI 3 program's mouse moves no longer look
  for MUI 4/5 pointer shapes. Programs see no difference: the same
  attributes, methods, notifications and pixels, and the same speed.

## 0.1b2 (2026-11-13, the first public beta)

The first OpenMUI archive published. It contains everything in 0.1b1 below
(which went only to testers) and these changes.

### Added

- **Spot's UP-MCC 1.0 is bundled**: eleven flat-look classes (UP-Button,
  UP-Text, UP-Panel, UP-Tabs, UP-Balance, UP-Slider, UP-Progress, UP-Cycle,
  UP-Field, UP-Scroller, UP-List), UP-MCC.mcp, `up-mcc.library`, the Inter
  font and the Dark and Light themes, byte-identical to Spot's release
  (github.com/spotUP/selecta; MIT, Lucide icons ISC, Inter SIL OFL 1.1). The
  installer puts them in place only where you have no same or newer copy,
  and writes no theme file of its own. Installer 2.4.

- OpenMUISwitch has a catalog (`OpenMUISwitch.catalog`, Swedish included):
  its messages, requesters and gadget texts follow the user's language;
  without locale.library or a catalog the English built in is used.
  The program grew by 968 bytes (13112 to 14080, stripped).
- The "MUI version" group of the prefs page (Behave as, the program list,
  Add/Remove and the help texts) is translatable: 8 new ids in the prefs
  catalog, Swedish added, other languages show English until translated.
- Release check that the five catalogs of OpenMUI's own texts (library,
  prefs program, PrefsWindow, AboutWindow, SystemPrefsWindow) each have a
  Swedish (`svenska`) catalog in the package. The Swedish translations
  themselves were already built into the package (from the AROS translation
  team's catalogs); they are now audited: complete, ISO-8859-1.

### Changed

- The five catalogs of OpenMUI's own texts are now part of the repository
  instead of git submodules of the AROS translation team (source URL and
  commit in `UPSTREAM.txt` in each folder; AROS Public License). TheBar
  stays a submodule.
- Swedish: Color is Färg, Mark is Markering, Top/Bottom are
  Överkant/Nederkant, and the tape image labels read naturally (Spela upp,
  Spela bakåt, Paus, Stopp, Spela in, Upp, Ner).
- **OpenMUI's own drawers are named OpenMUI** ("it's not Zune anymore"):
  the class files and modules are in `LIBS:OpenMUI` (was `LIBS:Zune`), the
  settings in `ENVARC:OpenMUI` and `ENV:OpenMUI` (was `ENVARC:Zune`,
  `ENV:Zune`: global and per-program settings, `BehaveAs`, window positions,
  font families, public screens), external images in `LIBS:OpenMUI/Images`,
  the catalogs `System/Prefs/OpenMUI.catalog` and `System/Classes/OpenMUI/`.
  The archive's `Libs/Zune` is `Libs/OpenMUI`. A real MUI's `ENV:MUI`, `MUI:`
  and `LIBS:MUI` are untouched as before.
- **Updating an earlier build moves its settings.** The installer (2.3)
  moves `ENVARC:Zune` and `ENV:Zune` to the new names (a file already there
  is not replaced; the old one stays), takes the earlier build's files out
  of `LIBS:Zune` with its record, and keeps anything else there (yours) in
  the backup, so that *Uninstall* and *Use real MUI* put it back.
  OpenMUISwitch 1.2 does the moving (`MIGRATE`); `UNINSTALL SETTINGS`
  removes both the new and the old settings drawers.
- If an earlier build's settings are still in `ENV:Zune` (OpenMUI copied in
  by hand), the library reads them, never writes them, until OpenMUI has
  its own; OpenMUI prefs copies them over the first time it runs. The
  library grew 200 bytes; the first program (MUI-Demo, FS-UAE) takes
  460,176 bytes (459,992 before, MUI 3.8 460,296); no stack frame grew.
- The Aminet pair is always `OpenMUI.lha` and `OpenMUI.readme`.
- **Faster than MUI 5 in all 27 operations measured** (opbench, OS 3.2.1,
  emulator), and as fast as MUI 3.8 in 24 of 26 (was 23): OpenMUI no longer
  asks the operating system for tag lists and for objects' own state on hot
  paths, a list scroll by one row draws one row, key releases are not
  converted, a slider reuses its text buffer. Creating a program's windows is
  now at MUI 3.8's speed.

### Fixed

- **A group paints the space between its items with its background**, also
  when the program set `MUIA_FillArea` to FALSE, as MUI 3.8 and MUI 5 do
  (UP-Panel's dark face was grey with a black box behind every label).
- **Setting a group's spacing, direction, columns or rows no longer reaches
  the groups inside it**, as on MUI 3.8 and MUI 5 (UP-Demo was far too tall).
- **A text field's own background**: programs that get MUI 4/5 keep the
  String's `MUIA_Background`, as on MUI 5; programs that get MUI 3.8 can
  recolour the field through its gadget, as on MUI 3.8 (UP-Field's grey
  field hid its grey placeholder text). The MUI 3.8 part loads only when a
  program uses it; the three fixes cost 68 bytes of library.

## 0.1b1 (test builds for testers, not published)

OpenMUI is the lean edition of Zune68 (Stefan Reinauer's port of AROS's Zune
to AmigaOS 3), renamed on 2026-11-11. Earlier test builds were called "Zune68k
lean". This entry sums up the work from the lean edition's start (2026-11-04)
to 2026-11-12. Details, measurements and the reasons are in
[docs/lean/README.md](docs/lean/README.md).

### Added

- **Each program gets the MUI it asks for.** The library reports MUI 5
  (21.227) and hands each program a base for its identity: MUI 3.8 (19.35),
  MUI 4 (20.6000) or MUI 5. OpenMUI prefs, System page: "Behave as"
  Automatic / MUI 3.8 / MUI 4 / MUI 5 for all programs, and a list of programs
  with a MUI of their own (`ENV:Zune/BehaveAs`).
- **Installer** that leaves a real MUI alone, writes down every file it
  installs, keeps every file it replaces, installs a bundled class only when
  you have no same or newer one, and puts OpenMUI in the first `LIBS:` drawer.
  `SYS:OpenMUI` has *Use real MUI*, *Use OpenMUI* and *Uninstall*
  (`OpenMUISwitch`, also from a Shell). An earlier "Zune68k lean"
  installation is removed first.
- **Classes and rarely used library parts as files** in `LIBS:Zune`, loaded
  the first time a program needs them (class files `<Name>.mui`, modules
  `Z<name>.mui`).
- **MUI 4/5 support behind that rule** (a MUI 3 program pays nothing):
  Hyperlink, MUI 5's compressed ("Z") images in Pixmap and Rawimage, MUI 5's
  scrollable window contents for a window larger than the screen, elided
  texts, `MUIM_FindObject`, `MUIA_FrameVisible`/`MUIA_FrameDynamic`, List's
  sort mark, Title's register tabs, drawn pointer shapes.
- **Settings:** import of MUI 3.8's settings files (read only); settings pages
  for Listtree and Busy; the ARexx switch, the drag, iconification, popup
  speed and active-object-colour settings are honoured.
- **External BOOPSI image classes** (`3:<name>.image` image specs) and a
  faster External page in prefs.
- **Test tools:** the real-program suite (132 programs and 60 class demos,
  against real MUI 3.8 and MUI 5), pixel comparison of the demos, `failn`
  fault injection, soak, start/quit loops, 68000 and RTG runs, fuzzers for
  settings files, ARexx and the string parsers, `opbench`, `memtally`.
- A production-grade plan with measurable gates: [docs/production.md](docs/production.md).
- This changelog, the [user guide](docs/user-guide.md) and the
  [developer guide](docs/developer-guide.md).

### Changed

- **Smaller library:** the library file went from 429 KB (Zune68) to about
  256 KB, with about 45 class and module files beside it. Several rounds
  split the big classes (List, Window, String, Group, Area, Application) so
  their dispatchers pick the method, and moved MUI 4/5-only and AROS-only
  code into the `Zmui45` module.
- **Memory** (MUI-Demo, OS 3.1 + 4 MB fast, FS-UAE): the first program 460,208
  bytes against MUI 3.8's 460,296; each further program about 204.5 KB against
  MUI 3.8's 210.6 to 218.9 KB.
- **Speed** (opbench, 26 operations, cycle-exact FS-UAE, the same binary on
  both libraries): 23 at least as fast as MUI 3.8 after two rounds (19 after
  the first); creating an application is 1.06 to 1.08 times MUI 3.8's.
- **MUI 3.8's behaviour** in many small places, found against real MUI 3.8
  by comparing programs: window sizes and gadgets, the spare pixels of
  groups, frames, fonts, popups, Coloradjust, Busy, Register, List scrollers,
  the root object's frame, how `OM_NEW` sets attributes.
- **One library.** `zunemaster.library` (the same library under a second name)
  is no longer built or installed; the installer takes an old one away. The
  prefs program opens `muimaster.library` only, so prefs beside a running
  program costs 244 KB less (FS-UAE).
- **The rename:** the library's texts, the prefs program (`SYS:Prefs/OpenMUI`),
  the installer drawer (`SYS:OpenMUI`), the switch tool (`OpenMUISwitch`) and
  the archive (`OpenMUI-0.1b1-DATE-amigaos3-m68k.lha`). The library file
  `muimaster.library` and its version numbers keep their names; `LIBS:Zune`,
  `ENV:Zune` and `ENVARC:Zune` were renamed later (see Unreleased).
- GitHub workflows and release tools name OpenMUI.

### Fixed

- **Compatibility round 1** (48 more programs, 49 class demos): BWin's window
  never opened (Window's `OM_NEW` set its tags through the subclass); NFS-GUI
  ended before its window (Register's spacing in the fitting stages).
- **Compatibility round 2:** TipOfDay's title (Window's `OM_GET` stored 0 for
  an unknown attribute), the `OM_NEW` audit.
- **Compatibility round 3:** frames, Coloradjust, List without its own
  scroller, Busy's size and frame, the root object's frame, Rectangle bar
  titles.
- **Compatibility round 4:** a program's identity lost after another program
  had used a class library; MUI 5's bar titles, fonts, List scroller and Busy
  size for MUI 4/5 programs; the
  iconify-at-startup setting.
- **Stability round 1 and 2:** found by failing the Nth resource call, hours
  of soak, start/quit loops over 181 programs and demos (705 cycles), runs on a
  stock 68000 and on an RTG screen, and fuzzers. Fixed: a failed object
  creation left its children behind (Group, Family, Window, Application and
  others); a double dispose in Popstring and Listview parts; a flush after a
  class file could not be loaded; the library stayed unexpungeable after OS
  3.1's `MakeClass` failed; Popasl without asl.library; a shared class open
  that did not set `SysBase`; a popup now opens on its parent window's screen.
- **Code audit** of the whole library: six findings fixed (Configdata,
  PanelTitle, Gauge, List size, Boopsi get, Popasl parts), and the library
  builds with no `-Wall` warnings.
- **`MUI_CreateCustomClass`:** the supername wins over supermcc (LumiZip reset
  at quit).
- **The AnimMaker/Register round:** the Register page pixel (the line under
  an erased tab); AnimMaker's resets were measured as its own bug (the same
  rate on MUI 3.8).
- **Prefs:** it no longer opens a real MUI's own classes (it reset a MUI 5
  machine); opens in 6 s instead of 84 s in the emulator.

### Known problems

See "Known differences from MUI" in the [developer guide](docs/developer-guide.md#known-differences-from-mui).
Voyager 3.5.3 can reset the machine after a quit by its close gadget (its own
blit; reproducible only on OpenMUI). Not yet measured on real hardware.
