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

**Version 0.1b2, public beta.** All figures in this document come from
cycle-exact emulation (FS-UAE) of an A1200 and an A600. They have not been
measured on a real Amiga.

## Statistics

![Headline figures](docs/stats/headline.png)

### Speed against MUI 3.8

![OpenMUI's time as a share of MUI 3.8's, 26 operations](docs/stats/speed-mui38.png)

### Speed on an A600 against MUI 3.8

![OpenMUI's time as a share of MUI 3.8's on an A600, 26 operations](docs/stats/speed-a600.png)

### Speed against MUI 5

![OpenMUI's time as a share of MUI 5's, 27 operations](docs/stats/speed-mui5.png)

### Memory

![Memory MUI-Demo takes on OpenMUI, MUI 3.8, MUI 5 and Zune68](docs/stats/memory.png)

### Against the original Zune68

![OpenMUI against the original Zune68](docs/stats/zune68.png)

### Progress

![Operations at least as fast as MUI 3.8, round by round](docs/stats/progress.png)

### Compatibility and stability

![Compatibility and stability figures](docs/stats/stability.png)

All values, with exact numbers on hover and table views:
[docs/stats/openmui-stats.html](docs/stats/openmui-stats.html) (download and
open in a browser). The pictures and the tables below show the measurements of
2026-11-19 (Zune68's figures: 2026-11-12). Measurement methods:
[docs/lean/measurements.md](docs/lean/measurements.md).

## Memory

MUI-Demo cold start, bytes of RAM taken. Measured 2026-11-19: OS 3.1 with
4 MB fast RAM (MUI 5: OS 3.2.1), FS-UAE, 68000 build.

| | OpenMUI | MUI 3.8 | MUI 5 |
|---|---:|---:|---:|
| First program (library and classes loaded) | 428.2 KB | 460.3 KB | 1,293.7 KB |
| Each further program | 193.1 KB | 210.6-218.9 KB | 427-435 KB |
| Library alone in memory | 246,088 bytes | 238,520 bytes | 581,024 bytes |

With MUI 5 behaviour (OS 3.2.1) OpenMUI takes 434.8 KB for the first
program, 201.7 KB for each further one and 249,968 bytes for the library
alone.

Files on disk (stripped): OpenMUI's library 232,200 bytes plus 54 class files
in `LIBS:OpenMUI` (289,948 bytes, loaded only when used); MUI 3.8's 166,412
plus 36 class files (271,208); MUI 5's 565,488 plus 80 (2,053,912).

## Speed

Milliseconds per operation, measured 2026-11-19: median of 9 repetitions, the same test
program on every library, cycle-exact FS-UAE emulation of an A1200 (68EC020,
14 MHz) and an A600 (68000, 7 MHz), OS 3.1 with 4 MB fast RAM, two instances
of each library, swapped. Lower is better. Ranges are the spread over the two
instances.

| Operation | A1200 OpenMUI | A1200 MUI 3.8 | A600 OpenMUI | A600 MUI 3.8 |
|---|---:|---:|---:|---:|
| Application, empty | 15.9-16.0 | 90-92 | 66-67 | 353-354 |
| Application with windows, create | 189-190 | 235 | 753-764 | 931-932 |
| Window open + close | 399-401 | 520-521 | 1,381-1,390 | 1,754-1,761 |
| Full redraw | 133 | 140-151 | 441-442 | 470-474 |
| Relayout | 172-173 | 197-207 | 608 | 698-702 |
| Slider set | 2.7 | 4.0-4.1 | 10.1 | 15.1-15.7 |
| Notification chain | 5.6 | 7.8-7.9 | 20.9 | 29.4-30.6 |
| Cycle set | 2.0 | 2.4 | 7.8 | 9.3 |
| Radio set | 8.3 | 15.6 | 32.3 | 60 |
| Text set | 2.4 | 2.5 | 9.4 | 9.7 |
| List insert, 100 rows | 61-62 | 831-832 | 226-230 | 2,739-2,743 |
| List insert, 1,000 rows | 249 | 6,648-6,651 | 1,008-1,017 | 23,912-23,914 |
| List sort, 1,000 rows | 94-95 | 658 | 379-386 | 2,488-2,490 |
| List scroll, one row | 13.2-13.3 | 14.6 | 37.0-37.4 | 39.9-40.0 |
| Button click | 16.6-16.9 | 22.4-22.6 | 69 | 91-92 |
| String, one key | 6.7 | 7.8-7.9 | 26.2-26.3 | 31.4-31.7 |
| String, backspace | 6.5-6.6 | 7.7 | 25.8 | 31.1-31.3 |
| Register page | 42.6 | 54 | 145 | 160 |
| Popup open + close | 96 | 120 | 339-342 | 404-408 |
| Uniconify | 337-338 | 449 | 1,248 | 1,530-1,535 |

Operations not listed (menu, iconify, window resize, list active, list scroll
by page, list insert of one row, text set) take 0.79 to 0.99 of MUI 3.8's time
on the A1200 (menu 13.2 ms against 13.2). On the A600, list insert of one row
(32.5-33.0 ms against 32.4) and menu open + close (47.2-47.5 against 46.5-46.8)
are level with MUI 3.8's; the other 24 of 26 operations are faster. The complete
tables are in [docs/lean/measurements.md](docs/lean/measurements.md).

Against MUI 5 (A1200, OS 3.2.1, OpenMUI opened as MUI 5 version 21.227, same
conditions), OpenMUI is faster in all 27 operations measured (ratio 0.01 to
0.82):

| Operation | OpenMUI | MUI 5 | Ratio |
|---|---:|---:|---:|
| Application with windows, create | 201-203 | 577 | 0.35 |
| Window open + close | 442 | 1,028-1,030 | 0.43 |
| Full redraw | 139 | 206 | 0.67 |
| Cycle set | 2.0-2.1 | 2.7 | 0.75 |
| Radio set | 8.4 | 11.9 | 0.71 |
| List insert, 1,000 rows | 249 | 20,201-20,206 | 0.01 |
| Button click | 16.7-16.9 | 20.8-21.2 | 0.79-0.80 |
| String, one key | 7.5 | 14.9-15.4 | 0.50 |
| Uniconify | 372-373 | 1,212-1,215 | 0.31 |
| Popup open + close | 104 | 527-538 | 0.20 |


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
