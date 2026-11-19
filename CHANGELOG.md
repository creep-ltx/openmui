# Changelog

Changes to OpenMUI, newest first. Betas are numbered `X.YbN`. All figures come
from cycle-exact emulation (FS-UAE; Amiberry for compatibility runs), not from a
real Amiga. A1200 = 68EC020 14 MHz, A600 = 68000 7 MHz, OS 3.1, 4 MB fast RAM.

OpenMUI is not MUI and is not affiliated with MUI's authors.

## Unreleased

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

## 0.1b2 (2026-11-13, first public beta)

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
