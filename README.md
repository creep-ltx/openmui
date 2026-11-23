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
second round of that day (settings shared between programs, smaller objects,
list inserts, mouse moves; see [CHANGELOG.md](CHANGELOG.md)): median of 9
repetitions, the same test program on every library, cycle-exact FS-UAE
emulation of an A1200 (68EC020, 14 MHz) and an A600 (68000, 7 MHz), OS 3.1 with
4 MB fast RAM, two instances of each library. Zune68 is Beta 5 (2026.11.22) as
released. Lower is better. Ranges are the spread over the two instances.

| Operation | A1200 OpenMUI | A1200 MUI 3.8 | A1200 Zune68 | A600 OpenMUI | A600 MUI 3.8 | A600 Zune68 |
|---|---:|---:|---:|---:|---:|---:|
| Application, empty | 9.6-9.8 | 92 | 14.4-14.7 | 37-38 | 356-361 | 62-63 |
| Application with windows, create | 177-180 | 236-237 | 264-265 | 709-729 | 935-937 | 1,055-1,081 |
| Window open + close | 400-401 | 521-522 | 601-604 | 1,389-1,392 | 1,761-1,765 | 2,177-2,196 |
| Full redraw | 133 | 151 | 154 | 442 | 471 | 532-533 |
| Relayout | 173 | 207 | 309-310 | 608-609 | 699 | 1,155-1,169 |
| Slider set | 2.7 | 4.0 | 4.4 | 10.1 | 15.7 | 16.1 |
| Notification chain | 5.6 | 7.8 | 9.1 | 21 | 31 | 33 |
| Cycle set | 2.0 | 2.4 | 4.7-4.9 | 7.9 | 9.3-9.4 | 19.0-19.3 |
| Radio set | 8.3 | 15.6 | 21 | 32 | 60-61 | 82 |
| Text set | 2.4 | 2.5 | 2.4 | 9.3 | 9.7 | 9.4 |
| List insert, one row | 7.3-7.4 | 10.3-10.4 | 34-35 | 29 | 32 | 126-127 |
| List insert, 100 rows | 61-62 | 830-831 | 109 | 224-226 | 2,745-2,746 | 413-422 |
| List insert, 1,000 rows | 248-249 | 6,651-6,654 | 674-683 | 1,008-1,013 | 23,956-23,961 | 2,702-2,787 |
| List sort, 1,000 rows | 95-96 | 659 | 110-112 | 385-386 | 2,497-2,505 | 435-440 |
| List scroll, one row | 13.3-13.4 | 14.6 | 18.3-18.4 | 37 | 40 | 56-58 |
| Button click | 16.1 | 23 | 17.9-18.0 | 67 | 92 | 74 |
| String, one key | 6.6 | 7.5-8.0 | 10.8 | 26 | 32 | 44 |
| String, backspace | 6.5 | 7.4-7.9 | 10.4-10.5 | 26 | 32 | 42-43 |
| Register page | 43 | 54 | 60-61 | 146 | 161-164 | 216-224 |
| Popup open + close | 95-96 | 120 | 140 | 341-342 | 404-408 | 481-499 |
| Uniconify | 338-339 | 446-449 | 1,036 | 1,217-1,248 | 1,543 | 3,944-4,058 |

Operations not listed (menu, iconify, window resize, list active, list scroll
by page) are in [docs/lean/measurements.md](docs/lean/measurements.md). OpenMUI
is at or below MUI 3.8's time in all 26 operations on the A1200 and on the A600
(menu open + close 0.99 on both; list insert of one row 0.89 on the A600, where
it was 1.02 in release 1.0). Zune68 Beta 5 takes less time than MUI 3.8 in 7 of
the 26 on the A1200 (empty application, list insert of 100 and 1,000 rows,
list sort, button click, list scroll by page, text set) and in 9 on the A600
(also menu open + close and iconify). Against OpenMUI, Zune68 takes 1.01 to
4.73 times OpenMUI's time in every operation on the A1200, and 1.01 to 4.37
times in 25 of 26 on the A600; there its menu open + close is 0.87 of
OpenMUI's. That operation stops when the program has handled the events:
Zune68 asks Intuition to wait for the program before it shows a menu
(IDCMP_MENUVERIFY, MUI 4/5 behaviour), and the menu then appears at
Intuition's next input tick, after the measurement has ended. Measured until
the menu has been shown and closed again (`menu shown+close`), the A600 takes
46.3 ms on OpenMUI, 46.5-48.7 on MUI 3.8 and 119-120 on Zune68 (A1200 13.1,
13.3 and 120); choosing the first item of a menu (`menu select`) takes
155-157 ms on OpenMUI, 156 on MUI 3.8 and 243-254 on Zune68 (A1200 46.0-46.2,
45.4-46.6 and 120-121).

An application's start with a full settings file (`ENV:OpenMUI/global.prefs`,
160 settings) takes 25 ms on the A1200 and 92 ms on the A600 once one program
has read the file (59 and 239 ms in release 1.0): the library keeps the parsed
settings for every program while the file stays the same.

Against MUI 5 (A1200, OS 3.2.1, OpenMUI and Zune68 opened as MUI 5 version
21.227, same conditions), OpenMUI is faster in all 27 operations measured (ratio
0.01 to 0.77), Zune68 in 24 of 27 (0.03 to 0.99); cycle set, radio set and list
insert of one row take 1.48 to 1.83 times MUI 5's time on Zune68. Zune68 takes
1.02 to 4.68 times OpenMUI's time in 26 of the 27; menu
open + close is level (20.0 ms on both: both ask for IDCMP_MENUVERIFY in MUI 5
behaviour):

| Operation | OpenMUI | MUI 5 | Zune68 | OpenMUI / MUI 5 | Zune68 / MUI 5 |
|---|---:|---:|---:|---:|---:|
| Application, empty | 11.6-11.7 | 303-304 | 15.4-15.6 | 0.04 | 0.05 |
| Application with windows, create | 191-202 | 578-579 | 268-270 | 0.34 | 0.47 |
| Window open + close | 440-444 | 1,023-1,029 | 639-640 | 0.43 | 0.62 |
| Full redraw | 139 | 206-207 | 159 | 0.67 | 0.77 |
| Relayout | 182 | 317 | 312-313 | 0.57 | 0.99 |
| Slider set | 3.0 | 6.8 | 4.8 | 0.45 | 0.70 |
| Notification chain | 6.2 | 12.8 | 9.7 | 0.49 | 0.76 |
| Cycle set | 2.1 | 2.7 | 4.0 | 0.75 | 1.48 |
| Radio set | 8.4 | 11.9 | 22 | 0.71 | 1.83 |
| Text set | 1.6 | 2.3 | 1.7 | 0.72 | 0.74 |
| List insert, one row | 7.3 | 23 | 34 | 0.32 | 1.50 |
| List insert, 100 rows | 59-61 | 2,036-2,040 | 106-107 | 0.03 | 0.05 |
| List insert, 1,000 rows | 247-248 | 20,244-20,302 | 670-678 | 0.01 | 0.03 |
| List sort, 1,000 rows | 138 | 2,308 | 153-154 | 0.06 | 0.07 |
| List scroll, one row | 14.6 | 21 | 16.8 | 0.69 | 0.80 |
| Button click | 16.2 | 21 | 18.2 | 0.76 | 0.86 |
| String, one key | 7.5 | 15.3-15.4 | 10.8 | 0.49 | 0.71 |
| String, backspace | 7.4 | 14.6-14.7 | 10.4-10.5 | 0.50 | 0.71 |
| Register page | 45 | 89 | 54 | 0.51 | 0.61 |
| Popup open + close | 104-105 | 549-557 | 140 | 0.19 | 0.25 |
| Uniconify | 371-373 | 1,223-1,235 | 1,063-1,064 | 0.30 | 0.87 |
| Text shorten set | 3.1 | 4.7 | 3.2 | 0.66 | 0.68 |
| Menu open + close | 19.9-20.0 | 26 | 20.0 | 0.77 | 0.77 |


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
