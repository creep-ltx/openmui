# Changelog

All notable changes to OpenMUI. Betas are numbered `X.YbN` until OpenMUI has
been in daily use for a while. Figures are from emulators (Amiberry, FS-UAE),
not from a real Amiga.

OpenMUI is not MUI and is not affiliated with MUI's authors.

## Unreleased

### Added

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

(No 0.1b1 archive or tag has been published at the time of writing;
everything below is in the first beta.)

## 0.1b1 (first OpenMUI beta)

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
  `muimaster.library`, its version numbers, `LIBS:Zune`, `ENV:Zune` and
  `ENVARC:Zune` keep their names for now.
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
