# Changelog

Changes to OpenMUI, newest first. Betas are numbered `X.YbN`. All figures come
from cycle-exact emulation (FS-UAE; Amiberry for compatibility runs), not from a
real Amiga. A1200 = 68EC020 14 MHz, A600 = 68000 7 MHz, OS 3.1, 4 MB fast RAM.

OpenMUI is not MUI and is not affiliated with MUI's authors.

## Unreleased

- The MUI pens (shine, half shine, background, half shadow, shadow, text,
  fill, mark) can be set in the settings, Special page (spotUP). Unset, each
  is the screen's pen as before; set, a dark theme on a plain Workbench
  screen no longer draws black text. OpenMUI's own setting ids 0x50c-0x513;
  MUI 3.8 has none there.
- Installer: a class of the same version left by an earlier OpenMUI or Zune68
  is now updated (the old file goes to the backup). Before, it was kept as
  the user's own, so older builds stayed. Same-version classes from anyone
  else are still kept. OpenMUISwitch 1.4.

## 1.1b1 (2026-11-26)

### Added

- `opbench` (tests/native): `menu shown+close` and `menu select`, which wait
  until Intuition has shown and closed the menus. `menu open+close` stops when
  the program's loop has nothing more to do, which for a toolkit that asks for
  IDCMP_MENUVERIFY is before its menu is shown.

### Changed

- One central release version: the file `VERSION`. The library, the
  preferences program, OpenMUISwitch, the installer, the readmes and the
  archive name take it from there; `make check` fails on a hard-coded one.
- UP-MCC 1.0 (spotUP) stays bundled, as released. UP-MCC.mcp is version 1,
  so its page stays hidden in the preferences. Installer 2.5.
- Code inherited from Zune that could not run on AmigaOS 3 is gone: the
  public screen list of muiscreen.library (which the library never opened)
  with the file notification every program started for it, and the custom
  scrollbar knob of AROS. Rectangle's bars are drawn by smaller code. The
  library is 2.2 KB smaller; every Prop object 54 bytes, every Cycle 16 and
  every program's settings object 152; MUI-Demo needs 3.5 KB less for its
  first copy and 1.3 KB less for each further one. Same pixels, same speed
  (emulated A600, A1200 with a 68020 and with a 68030, OS 3.2.1).
- The settings file `ENV:OpenMUI/global.prefs` is read and parsed once for
  all programs. The library keeps the parsed settings while the file's date,
  size and key stay the same; each program checks those (three DOS calls)
  instead of loading the file. A program's own file (`<base>.prefs`) is still
  read by that program and laid over the shared settings. With a full
  settings file (160 settings) an application's start takes 25 ms instead of
  59 on an emulated A1200 and 92 instead of 239 on an A600. A changed or
  deleted file is used by the next program that starts, as before. Programs
  without a settings file are not affected.
- Inserting a row into a visible list is faster: 7.3 ms instead of 8.2 on the
  A1200 (MUI 3.8: 10.3), 29 ms instead of 33 on the A600 (MUI 3.8: 32). The
  list's own notification attributes (`MUIA_List_InsertPosition` and four
  others) are no longer handed to its scrollbars and their arrows.
- A button click takes 16.1 ms instead of 16.5 on the A1200 (MUI 3.8: 22.7)
  and 67 instead of 69 on the A600: the pressed object's event handler is
  changed in place instead of removed and added.
- Less memory per program: Text objects are 8 bytes smaller, Image objects 8,
  Rectangle objects 8, Prop objects 8, a Gauge without an info text 248, and
  every object with a notification 8. MUI-Demo needs 427.3 KB as the first
  program (was 428.0) and 188.6 KB as each further one (was 191.9); with MUI
  5 behaviour on OS 3.2.1 433.9 and 197.2 KB (were 434.7 and 200.5). The
  library file is 234,760 bytes stripped (was 232,084).
- Groups: when fewer pixels are left over than children can still grow,
  each of them gets one and the excess is taken from the first child on, as
  MUI 3.8 does (a first child at its maximum of 20 in a 135 pixel row:
  19/58/58, was 20/57/58). Of 3,543 layouts measured on MUI 3.8, 3,213 are
  now the same (were 3,117); none that was the same differs now.

- An application that reads no settings file (no `ENV:OpenMUI/global.prefs`
  and no file of its own) no longer builds the default settings: the first
  one builds them and keeps them for every later one (until the library is
  expunged). Starting an empty application takes 9.7-9.9 ms instead of 16.0
  on an emulated A1200 (Zune68 Beta 5: 14.4-14.6) and 37-38 instead of 66 on
  an A600 (Zune68: 61-63); with MUI 5 behaviour on OS 3.2.1 11.0-11.3 instead
  of 18.2-18.7 (Zune68: 14.9-15.3). Each further program needs 1,200 bytes
  less (191.9 KB with MUI-Demo); the first program the same (428.0 KB).
- A Text's new contents draw their background without going through the
  object's dispatcher when no class of it could change the background: text
  set 2.37 ms instead of 2.39 (A1200), 9.31-9.33 instead of 9.40-9.43 (A600),
  1.63 instead of 1.66 (OS 3.2.1, MUI 5 behaviour).
- MUI 5's shortened text (`MUIA_Text_Shorten`, in `Zmui45.mui`) measures with
  the object's own RastPort and keeps its elided pieces on the stack for a
  one-piece text in the object's font: 3.12 ms per set instead of 3.43-3.45
  (Zune68: 3.21-3.22).
- The default-settings tables use 16-bit fields (0.4 KB less code), so the
  library stays the same size (232,084 bytes stripped).

- Measurements of 2026-11-23 against Zune68 Beta 5 and MUI 3.8 redone with
  this build (README, statistics page, `docs/lean/measurements.md`).
- The size of the class files in `LIBS:OpenMUI`: 289,948 bytes stripped for
  release 1.0 (the developer guide was right; the measurements of 2026-11-23
  said 359,108), 290,056 now.

### Fixed

- Group: nested `MUIM_Group_InitChange` calls are counted, as in MUI 3.8:
  only the `ExitChange` that matches the first lays the group out again. An
  inner `ExitChange` laid out halfway through the outer change. (Found by
  the amigazen project's Mushin.)
- Window: setting only `MUIA_Window_Height` on a window that had been open
  reopened it at its minimum height; it now gets the requested height, as
  in MUI 3.8 and MUI 5. (Found by Mushin.)
- `MUI_RemoveClipRegion` (and `MUI_RemoveClipping`) installs the MUI region
  pushed before the removed one, or none, as MUI 3.8 and MUI 5 do; a region
  the program had installed itself came back instead. (Mushin's fix.)
- A window that is an AppWindow (`MUIA_Window_AppWindow`) is removed from
  Workbench's list when it closes. It never was: Workbench kept the closed
  window, and 64 bytes were lost each time.
- External images are also looked for in `LIBS:Zune/Images` (earlier
  installs) and, after it, `MUI:Images`; the External lists of the
  preferences show all three drawers merged (the first drawer wins a name).
  After an update over a hand-copied older build the lists were empty. The
  installer and `OpenMUISwitch MIGRATE` (1.3) copy `LIBS:Zune/Images` to
  `LIBS:OpenMUI/Images` when that does not exist, with or without an
  earlier installer record; the copy is not recorded, so Uninstall leaves
  it. OpenMUI ships no image sets.

- The preferences program no longer stops answering when UP-MCC's page is
  chosen. Like MUI 3.8 and MUI 5 it lists a settings class (`.mcp`) only
  from version 11 on (measured on both: 10 hidden, 11 shown); UP-MCC.mcp 1.0
  is version 1, so its page is not listed, as with MUI. That page waits
  forever on every MUI (checked on MUI 5 with a copy patched to version 11):
  it reads its theme with libnix's `fopen`, whose
  `malloc` locks a semaphore the class library never set up. OpenMUI's own
  Busy.mcp and Listtree.mcp are now version 11.1 so that they stay listed.

## 1.0 (2026-11-19)

The first release.

### Added

- Programmer reference (`docs/reference/`): the `muimaster.library` functions
  with vector offsets, every built-in class with attributes and methods
  (generated from the sources by `tools/gen_reference.py`), writing custom and
  external classes, differences from MUI 3.8 and MUI 5. Example class
  `CustomClass.c` in `SDK/examples`.
- The release archive contains the user guide, developer guide, programmer
  reference and changelog as plain text in `Docs/OpenMUI`.
- Class file `LIBS:OpenMUI/Zstring.mui` (clipboard, default Cut/Copy/Paste menu,
  BetterString's Insert) and `LIBS:OpenMUI/Zapp.mui` (ARexx commands, iconified
  icon, Exchange commands, dropped icons, help, About and settings windows),
  loaded on first use. MUI 4/5 text colours and effects (`\33P[RRGGBB]`,
  outline, shadow, ghost) are in `Zmui45.mui`.

### Changed

- **Rewrites for 68k machines** (programs see the same attributes, methods and
  pixels):

  | Class | Effect |
  |---|---|
  | Window | no pool of its own (1 KB less per opened window); code 3.1 KB (12.7 %) smaller; first MUI-Demo 448,488 bytes |
  | List | rows in one compact array (8 bytes per row); inserting 1,000 rows 248 ms (MUI 3.8: 6,650 ms); sort of 1,000 rows 94 ms (MUI 3.8: 658 ms); about 7 KB less memory per MUI-Demo |
  | Area | background and frame in one step in the common case; A1200 full redraw 134 ms (MUI 3.8: 141-151); A600 445 ms (MUI 3.8: 470-477); library code 1.4 KB smaller |
  | Group | one layout routine for both directions; 36 bytes per group instead of 68 (MUI 3.8: 42); A1200 relayout 4-5 % faster |
  | Application | A1200 application with windows 183-189 ms (MUI 3.8: 236), A600 758 ms (MUI 3.8: 929-948); empty application 16 ms (MUI 3.8: 92); a full settings file loads in 57 ms instead of 321 ms (A600: 237 ms instead of 1.37 s); code 29 % smaller |
  | String | 96 bytes per gadget instead of 308 (MUI 3.8: 88); A1200 typed key 6.7 ms (MUI 3.8: 7.5-7.9), A600 26.3 ms (30.7-31.5) |
  | Text engine, images, frames | library 7.5 KB smaller; radio set 2-3 % faster; A1200 radio set 8.3 ms (MUI 3.8: 15.6) |

- Memory, MUI-Demo cold start (MUI 3.8 in brackets): first program 428.2 KB
  (460.3 KB), each further program 193.1 KB (210.6-218.9 KB). Library file
  232,200 bytes stripped.
- Measured 2026-11-19 (FS-UAE, cycle-exact; build of that day): all 26 operations at or below MUI 3.8's
  time on an A1200 (0.04 to 0.99) and 24 of 26 on an A600 (two level); all 27 below MUI 5's (0.01 to 0.82).
- Window opening, page switches and popups draw less: no repeated background
  paint, no clip regions where nothing can spill over, no wait for the next
  display frame when opening on the front screen. A1200 window open+close
  398 ms (MUI 3.8: 520-523), register page 42.5 ms (53.7), popup 95 ms (120).
- Area data keeps the inner spacing and control char at MUI 3.8's offsets
  (`MUIA_InnerLeft/Top/Right/Bottom` at 88-91, control char at 96); the
  structure stays 108 bytes.
- Groups share spare pixels as MUI 3.8 does: 1,220 of 1,346 chosen layouts and
  1,133 of 1,399 random layouts match pixel for pixel (before: about 70 %).
- MUI 4/5 programs get MUI 5's window look: 8 pixels inside every window edge;
  a window larger than the screen has its scrollbars in the border. MUI 3
  programs are unchanged.

### Fixed

- A window froze the machine when an object 0 pixels wide or high sat in a group
  with a pattern background. Nothing is drawn into an empty box now.
- Tab order: objects on hidden pages and disabled objects are skipped, the order
  wraps, Tab with no active object starts at the first.
  `MUIM_Window_SetCycleChain` follows MUI 3.8.
- The HTMLtext demo's window opens at MUI 3.8's size (142x83 against 143x83).
- Custom frames from the settings (theme frame pictures) are drawn.
- Window border scroll arrows (`MUIA_Scrollgroup_UseWinBorder`) scrolled
  nothing. They scroll 8 pixels per click and repeat when held; the border
  knobs show the position. A Scrollgroup's own arrows step 8 pixels (were 1).
- Scrolling a large virtual group: 8 pixels of a group of 80 buttons took 2.9 s
  on a 68020 (MUI 3.8: 71 ms), now 93 ms.
- A settings file whose strings lack terminators no longer makes the library read
  past a setting.

## 0.1b2 (2026-11-13, private beta)

Includes everything in 0.1b1.

### Added

- Spot's UP-MCC 1.0, byte-identical to its release (github.com/spotUP/selecta;
  MIT, Lucide icons ISC, Inter SIL OFL 1.1): UP-Button, UP-Text, UP-Panel,
  UP-Tabs, UP-Balance, UP-Slider, UP-Progress, UP-Cycle, UP-Field, UP-Scroller,
  UP-List, UP-MCC.mcp, `up-mcc.library`, the Inter font, Dark and Light themes.
  Installed only where no same or newer copy exists. Installer 2.4.
- `OpenMUISwitch.catalog` (Swedish included); the program grew 968 bytes.
- Translatable "MUI version" group in the preferences (8 new catalog ids,
  Swedish included).
- Release check that the five catalogs of OpenMUI's own texts each have a
  Swedish catalog in the package.

### Changed

- Own drawers renamed: `LIBS:OpenMUI` (was `LIBS:Zune`), `ENVARC:OpenMUI` and
  `ENV:OpenMUI` (were `ENVARC:Zune`, `ENV:Zune`), `LIBS:OpenMUI/Images`,
  catalogs `System/Prefs/OpenMUI.catalog` and `System/Classes/OpenMUI/`.
  The archive's `Libs/Zune` is `Libs/OpenMUI`. A real MUI's `ENV:MUI`, `MUI:`
  and `LIBS:MUI` are untouched.
- Updating a test build moves its settings (installer 2.3, OpenMUISwitch 1.2
  `MIGRATE`): a file already present is not replaced; the build's files leave
  `LIBS:Zune`; other files there go to the backup. `UNINSTALL SETTINGS` removes
  both settings drawers. Settings still in `ENV:Zune` are read, never written,
  until OpenMUI has its own.
- The five catalogs of OpenMUI's own texts are in the repository (source and
  commit in each `UPSTREAM.txt`; AROS Public License) instead of submodules.
  TheBar remains a submodule.
- Swedish: Color is Färg, Mark is Markering, Top/Bottom are
  Överkant/Nederkant, tape image labels reworded.
- The Aminet pair is `OpenMUI.lha` and `OpenMUI.readme`.
- Speed: faster than MUI 5 in all 27 operations measured (OS 3.2.1) and as fast
  as MUI 3.8 in 24 of 26 (was 23). The library no longer asks the OS for tag
  lists and objects' own state on hot paths, a one-row list scroll draws one
  row, key releases are not converted, a slider reuses its text buffer.
  Creating a program's windows is at MUI 3.8's speed.
- Memory: first MUI-Demo program 460,176 bytes (MUI 3.8: 460,296); library
  grew 200 bytes for the settings migration.

### Fixed

- A group paints the space between its items with its background also when
  `MUIA_FillArea` is FALSE, as MUI 3.8 and MUI 5 do.
- Setting a group's spacing, direction, columns or rows no longer reaches the
  groups inside it, as on MUI 3.8 and MUI 5.
- A String's own background: programs that get MUI 4/5 keep `MUIA_Background`;
  programs that get MUI 3.8 can recolour the field through its gadget. The MUI
  3.8 part loads on use; the three fixes cost 68 bytes of library.

## 0.1b1 (2026-11-12, tester builds, not published)

First OpenMUI builds: the lean edition of Zune68 (work from 2026-11-04),
renamed from "Zune68k lean" on 2026-11-11.

### Added

- Per-program MUI identity: the library reports MUI 5 (21.227) and hands each
  program a base for MUI 3.8 (19.35), MUI 4 (20.6000) or MUI 5. Preferences,
  System page: "Behave as" Automatic / MUI 3.8 / MUI 4 / MUI 5 and a per-program
  list (`BehaveAs`).
- Installer that leaves a real MUI alone, records every installed file, keeps
  every replaced file, installs a bundled class only when no same or newer
  copy exists, and installs into the first `LIBS:` drawer. `SYS:OpenMUI` has
  *Use real MUI*, *Use OpenMUI* and *Uninstall* (`OpenMUISwitch`, also from a
  Shell). A "Zune68k lean" installation is removed first.
- About 45 classes and library parts as files loaded on first use (`<Name>.mui`
  classes, `Z<name>.mui` modules).
- MUI 4/5 support behind the identity or in class files: Hyperlink, compressed
  images in Pixmap and Rawimage, scrollable contents for a window larger than the
  screen, elided texts, `MUIM_FindObject`, `MUIA_FrameVisible`,
  `MUIA_FrameDynamic`, List sort marks, Title register tabs, drawn pointer
  shapes.
- Settings: import of MUI 3.8's settings (read only); settings pages for
  Listtree and Busy; the ARexx switch, drag, iconification, popup speed and
  active-object colour settings are honoured.
- External BOOPSI image classes (`3:<name>.image`), a faster External prefs page.
- Test tools: the real-program suite (132 programs and 60 class demos against
  real MUI 3.8 and MUI 5), pixel comparison of the demos, `failn` fault
  injection, soak and start/quit loops, 68000 and RTG runs, fuzzers for settings
  files, ARexx and the string parsers, `opbench`, `memtally`.
- Production gates ([docs/production.md](docs/production.md)), user guide,
  developer guide.

### Changed

- Library file 429 KB (Zune68) to about 256 KB, with about 45 class and module
  files beside it. The big classes were split so their dispatchers pick the
  method; MUI 4/5-only and AROS-only code moved to the `Zmui45` module.
- Memory (MUI-Demo): first program 460,208 bytes (MUI 3.8: 460,296), each
  further program 204.5 KB (210.6-218.9 KB).
- Speed (`opbench`, 26 operations): 23 at least as fast as MUI 3.8.
- Many details of MUI 3.8's behaviour: window sizes and gadgets, group spare
  pixels, frames, fonts, popups, Coloradjust, Busy, Register, List scrollers,
  the root object's frame, how `OM_NEW` sets attributes.
- One library: `zunemaster.library` is no longer built or installed; the
  installer removes an old one. Preferences beside a running program take
  244 KB less.
- Names: `SYS:Prefs/OpenMUI`, `SYS:OpenMUI`, `OpenMUISwitch`,
  `OpenMUI-0.1b1-DATE-amigaos3-m68k.lha`. The library keeps the name
  `muimaster.library` and MUI's version numbers.

### Fixed

- Compatibility round 1: BWin's window did not open (Window `OM_NEW` set its tags
  through the subclass); NFS-GUI ended before its window (Register spacing).
- Round 2: TipOfDay's title (Window `OM_GET` stored 0 for an unknown attribute);
  `OM_NEW` audit.
- Round 3: frames, Coloradjust, List without its own scroller, Busy size and
  frame, root object frame, Rectangle bar titles.
- Round 4: a program's identity was lost after another program used a class
  library; MUI 5 bar titles, fonts, List scroller and Busy size for MUI 4/5
  programs; the iconify-at-startup setting.
- Stability rounds 1 and 2 (failing the Nth resource call, soak, 705
  start/quit cycles over 181 programs and demos, stock 68000 and RTG runs,
  fuzzers): a failed object creation left children behind (Group, Family,
  Window, Application and others); a double dispose in Popstring and Listview
  parts; a flush after a class file could not be loaded; the library stayed
  unexpungeable after OS 3.1's `MakeClass` failed; Popasl without asl.library;
  a shared class open did not set `SysBase`; a popup now opens on its parent
  window's screen.
- Code audit of the library: six findings fixed (Configdata, PanelTitle, Gauge,
  List size, Boopsi get, Popasl parts); no `-Wall` warnings.
- `MUI_CreateCustomClass`: the supername wins over supermcc.
- Register page pixel (the line under an erased tab).
- Preferences no longer opens a real MUI's classes; opens in 6 s instead of 84 s
  in the emulator.

### Known problems

See [Known differences](docs/developer-guide.md#known-differences-from-mui).
Voyager 3.5.3 can reset the machine after a quit through its close gadget (its
own blit; reproducible only on OpenMUI).
