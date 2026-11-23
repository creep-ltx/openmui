# OpenMUI

OpenMUI is a free, open-source replacement for MUI (the Magic User Interface)
on classic AmigaOS 3.x. It provides `muimaster.library`, the built-in classes
and a preferences program, and runs programs written for MUI 3, 4 and 5. It
needs AmigaOS 3.1 or newer, a 68000 or better and no FPU. Its look and memory
use follow MUI 3.8.

> **OpenMUI is not MUI and is not affiliated with MUI's authors.** MUI was
> created by Stefan Stuntz; MUI 4 and 5 come from the MUI team. OpenMUI is a
> separate library that is compatible with MUI's programming interface. MUI 3.8,
> 4 and 5 were studied black-box only: their behaviour was measured and their
> public documentation read; their code was never read, disassembled or copied.

**Version 1.0.** All figures in this document come from
cycle-exact emulation (FS-UAE) of an A1200 and an A600. They have not been
measured on a real Amiga.

## Statistics

![Headline figures](docs/stats/headline.png)

### Speed

Milliseconds per operation, measured 2026-11-23 with the build after the
first speed round of 2026-11-23 (shared default settings, Text drawing, MUI 5
text shortening; see [CHANGELOG.md](CHANGELOG.md)): median of 9 repetitions,
the same test program on every library, cycle-exact FS-UAE emulation of an
A1200 (68EC020, 14 MHz) and an A600 (68000, 7 MHz), OS 3.1 with 4 MB fast RAM,
two instances of each library. Zune68 is Beta 5 (2026.11.22) as released.
Lower is better. Ranges are the spread over the two instances.

| Operation | A1200 OpenMUI | A1200 MUI 3.8 | A1200 Zune68 | A600 OpenMUI | A600 MUI 3.8 | A600 Zune68 |
|---|---:|---:|---:|---:|---:|---:|
| Application, empty | 9.7-9.9 | 91 | 14.4-14.6 | 37-38 | 352-359 | 61-63 |
| Application with windows, create | 178-185 | 235 | 256-269 | 727-735 | 935 | 1,061-1,082 |
| Window open + close | 399 | 520 | 582-602 | 1,390 | 1,761 | 2,181 |
| Full redraw | 133 | 141-151 | 153 | 442 | 474 | 532 |
| Relayout | 173 | 198-207 | 299-308 | 607 | 702 | 1,154-1,167 |
| Slider set | 2.7 | 4.0-4.1 | 4.4 | 10.1 | 15.1 | 16.1 |
| Notification chain | 5.6 | 7.8-7.9 | 9.1 | 21 | 29 | 33 |
| Cycle set | 2.0 | 2.4 | 4.5-4.9 | 7.8 | 9.3 | 19.2-19.4 |
| Radio set | 8.3 | 15.6 | 21 | 32 | 60 | 82 |
| Text set | 2.4 | 2.5 | 2.4 | 9.3 | 9.7 | 9.4 |
| List insert, 100 rows | 61-62 | 829 | 104-109 | 227 | 2,744 | 418-423 |
| List insert, 1,000 rows | 248 | 6,652 | 645-688 | 1,013 | 23,936 | 2,720-2,777 |
| List sort, 1,000 rows | 94 | 658 | 110 | 379-385 | 2,490 | 435 |
| List scroll, one row | 13.3 | 14.6 | 17.9 | 37 | 40 | 57-58 |
| Button click | 16.5 | 23 | 17.9 | 69 | 92 | 74 |
| String, one key | 6.7 | 7.8 | 10.8 | 26 | 30-31 | 44 |
| String, backspace | 6.6 | 7.7-7.8 | 10.4 | 26 | 30-31 | 43 |
| Register page | 43 | 54 | 58-61 | 145 | 161 | 219-223 |
| Popup open + close | 95 | 120 | 140 | 340 | 404-412 | 480-497 |
| Uniconify | 338 | 449 | 983-1,036 | 1,243 | 1,535 | 3,968 |

Operations not listed (menu, iconify, window resize, list active, list scroll
by page, list insert of one row) are in
[docs/lean/measurements.md](docs/lean/measurements.md). OpenMUI is below MUI
3.8's time in 25 of the 26 operations on the A1200 (menu open + close is level,
1.00) and in 25 of 26 on the A600 (list insert of one row is 1.02, as in
release 1.0). Zune68 Beta 5 takes less time than MUI 3.8 in 7 of the 26 on the
A1200 (empty application, list insert of 100 and 1,000 rows, list sort, button
click, list scroll by page, text set; iconify level) and in 9 on the A600 (also
menu open + close and iconify). Against OpenMUI, Zune68 takes 1.01 to 4.24
times OpenMUI's time in every operation on the A1200, and 1.01 to 3.84 times
in 25 of 26 on the A600; there its menu open + close is 0.87 of OpenMUI's.
That operation stops when the program has handled the events: Zune68 asks
Intuition to wait for the program before it shows a menu (IDCMP_MENUVERIFY,
MUI 4/5 behaviour), and the menu then appears at Intuition's next input tick,
after the measurement has ended. Measured until the menu has been shown and
closed again (`menu shown+close`, added to the test program), the A600 takes
46.6 ms on OpenMUI, 45.7 on MUI 3.8 and 120.0 on Zune68 (A1200 13.2, 13.2-13.3
and 120.0); choosing the first item of a menu (`menu select`) takes 159 ms on
OpenMUI, 157 on MUI 3.8 and 248 on Zune68 (A1200 47.0-47.5, 45.3-46.5 and
120.0).

Against MUI 5 (A1200, OS 3.2.1, OpenMUI and Zune68 opened as MUI 5 version
21.227, same conditions), OpenMUI is faster in all 27 operations measured (ratio
0.01 to 0.78), Zune68 in 24 of 27 (0.03 to 0.98); cycle set, radio set and list
insert of one row take 1.48 to 1.83 times MUI 5's time on Zune68. Zune68 takes
1.02 to 3.78 times OpenMUI's time in 26 of the 27; menu open + close is level
(20.0 ms on both: both ask for IDCMP_MENUVERIFY in MUI 5 behaviour):

| Operation | OpenMUI | MUI 5 | Zune68 | OpenMUI / MUI 5 | Zune68 / MUI 5 |
|---|---:|---:|---:|---:|---:|
| Application, empty | 11.0-11.3 | 300 | 14.9-15.3 | 0.04 | 0.05 |
| Application with windows, create | 187-192 | 574 | 265 | 0.33 | 0.46 |
| Window open + close | 438 | 1,006 | 636 | 0.43 | 0.63 |
| Full redraw | 139 | 206 | 159 | 0.67 | 0.77 |
| Relayout | 181 | 317 | 310 | 0.57 | 0.98 |
| Slider set | 3.0 | 6.8 | 4.8 | 0.45 | 0.70 |
| Notification chain | 6.2 | 12.8 | 9.7 | 0.49 | 0.76 |
| Cycle set | 2.1 | 2.7 | 4.0 | 0.75 | 1.48 |
| Radio set | 8.4 | 11.9 | 22 | 0.71 | 1.83 |
| Text set | 1.6 | 2.3 | 1.7 | 0.72 | 0.74 |
| List insert, 100 rows | 63 | 2,035 | 105 | 0.03 | 0.05 |
| List insert, 1,000 rows | 248 | 20,236 | 661 | 0.01 | 0.03 |
| List sort, 1,000 rows | 137 | 2,306 | 152 | 0.06 | 0.07 |
| List scroll, one row | 14.6 | 21 | 16.8 | 0.69 | 0.80 |
| Button click | 16.6 | 21 | 18.1-18.4 | 0.78 | 0.86 |
| String, one key | 7.5 | 15.3 | 10.8 | 0.49 | 0.70 |
| String, backspace | 7.4 | 14.6 | 10.5 | 0.51 | 0.71 |
| Register page | 45 | 89 | 54 | 0.51 | 0.61 |
| Popup open + close | 103 | 532 | 140 | 0.19 | 0.26 |
| Uniconify | 372 | 1,219 | 1,044-1,063 | 0.31 | 0.86 |
| Text shorten set | 3.1 | 4.7 | 3.2 | 0.67 | 0.69 |
| List insert, one row | 9.0 | 23 | 34 | 0.40 | 1.50 |
| Menu open + close | 20.0 | 26 | 20.0 | 0.77 | 0.77 |


## What it contains

- **`muimaster.library`**, built with GCC for the 68000, no FPU. The library
  file keeps MUI's name and version numbers because programs depend on them.
- **Per-program MUI identity.** Each program gets the MUI version it asks for:
  a MUI 3 program sees MUI 3.8 (19.35), a MUI 4 program MUI 4, a MUI 5
  program MUI 5 (21.227). The preferences have "Behave as" (Automatic, MUI 3.8,
  MUI 4, MUI 5) and a list of programs with a MUI of their own.
- **On-demand class files.** About 45 rarely used classes and library parts are
  separate files in `LIBS:OpenMUI`, loaded the first time a program needs one.
  A MUI 3 program pays nothing for MUI 4/5 features.
- **OpenMUI preferences** (`SYS:Prefs/OpenMUI`) with an import of MUI 3.8's
  settings.
- **Bundled MUI classes** with their preference pages: NList, NListtree,
  TextEditor, BetterString, TheBar, Busy, Listtree and others, and Spot's
  [UP-MCC](https://github.com/spotUP/selecta) 1.0 (eleven flat-look classes,
  `up-mcc.library`, the Inter font, Dark and Light themes; MIT).
- **Installer** that leaves a real MUI alone, keeps every file it replaces and
  provides *Use real MUI*, *Use OpenMUI* and *Uninstall* in `SYS:OpenMUI`.
- Swedish catalogs, an SDK with examples, the programmer reference and the
  full source.

## Installing

Unpack the archive `OpenMUI.lha` and double-click *Install* (Amiga Installer
43 or newer; on OS 3.1 use Installer 43.3 from Aminet). Details, switching to
a real MUI and uninstalling: [User guide](docs/user-guide.md). The drawers
OpenMUI uses are `LIBS:OpenMUI` (class files), `ENVARC:OpenMUI` and
`ENV:OpenMUI` (settings) and `SYS:Prefs/OpenMUI`.

## Compatibility

Tested in emulators against real MUI 3.8 and MUI 5 with 182 programs and class
demos (100 MUI 3 programs, 24 MUI 4/5 programs, 58 add-on class demos),
including YAM, AmIRC, AmFTP, IBrowse, NList, TextEditor and TheBar. 19 of
MUI's 27 demo windows are pixel-identical to MUI 3.8. Results:
[tests/lean/real-programs.md](tests/lean/real-programs.md). Known differences:
[developer guide](docs/developer-guide.md#known-differences-from-mui) and
[docs/reference/differences.md](docs/reference/differences.md).

![IBrowse 3.0 running on the Zune68 base, next to Zune Prefs](iBrowse.png)

## Building

Needs m68k-amigaos-gcc (GCC 16 used), GNU make, sfdc, fd2sfd, FlexCat and lha.

```sh
git submodule update --init --recursive
make -j4 all
make check     # needs the Python bundled with the Amiga toolchain
make release   # the archive and its Aminet readme, in build/muimaster/release
```

`make check` runs the linked 68000 code with modelled OS calls. It does not
establish GUI compatibility or speed on Amiga hardware.

## Documentation

| Document | Content |
|---|---|
| [User guide](docs/user-guide.md) | Installing, switching to a real MUI, uninstalling, preferences, "Behave as", troubleshooting |
| [Developer guide](docs/developer-guide.md) | Supported interface, identity mechanism, known differences, building, tests, clean-room rule, contributing |
| [Programmer reference](docs/reference/README.md) | Library functions, every class with attributes and methods, custom and external classes, differences from MUI 3.8 and MUI 5 |
| [Changelog](CHANGELOG.md) | Release history |
| [Production gates](docs/production.md) | The checkable criteria for version 1.0 |
| [docs/lean/](docs/lean/README.md) | The development log: rounds, measurements, class split, roadmap |
| [dist/Compatibility](dist/Compatibility) | Compatibility workloads and their limits |
| [tests/native/PERFORMANCE.md](tests/native/PERFORMANCE.md) | Speed measurement method |

The release archive contains the guides, the reference and the changelog as
plain text in `Docs/OpenMUI`.

## Origins

| | |
|---|---|
| **Zune** | The MUI-compatible GUI toolkit of [AROS](https://github.com/aros-development-team/AROS), by The AROS Development Team. |
| **Zune68** | Zune ported to classic AmigaOS 3 by Stefan Reinauer ([reinauer/zune68](https://github.com/reinauer/zune68)). |
| **OpenMUI** | The lean edition of Zune68: rewritten classes for 68000-class machines, with MUI 3.8's look, memory use and speed as the target and MUI 3, 4 and 5 programs as the workload. |

The AROS Public License applies to the core (`LICENSE`). The bundled classes
keep their own licences and notices (`LEGAL`, `ACKNOWLEDGEMENTS`). UP-MCC is
by spotUP (MIT; Lucide icons ISC; Inter font SIL OFL 1.1).

## Repository branches

- `aros-upstream` holds the filtered AROS history (component source, layout,
  authorship and dates retained; commit messages carry an `AROS-Commit`
  trailer). `main` is Zune68's work on top of it. `lean` is the OpenMUI line.
- The importer, the path list and the procedure for merging later AROS
  updates are in `tools/upstream/` (`import-aros.py`, `paths.txt`,
  `source.json`, `commit-map`). Use a complete AROS clone and a new output
  directory; never fetch the unfiltered AROS branch into `aros-upstream`.
- TheBar is a git submodule: run `git submodule update --init --recursive`
  after cloning.

GitHub Actions builds and checks every push. A date tag such as `v2026.11.11`
creates a draft pre-release with the archive and the Aminet readme.
