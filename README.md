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
open in a browser). The pictures show the measurements of 2026-11-12; the
tables below are current. Measurement methods:
[docs/lean/measurements.md](docs/lean/measurements.md).

## Memory

MUI-Demo cold start, bytes of RAM taken. OS 3.1 with 4 MB fast RAM
(MUI 5: OS 3.2.1), FS-UAE.

| | OpenMUI | MUI 3.8 | MUI 5 |
|---|---:|---:|---:|
| First program (library and classes loaded) | 428.2 KB | 460.3 KB | 1,293.7 KB |
| Each further program | 193.1 KB | 210.6-218.9 KB | 427-435 KB |
| Library alone in memory | 246,048 bytes | 238,520 bytes | - |

The OpenMUI library file is 232,200 bytes (stripped, 68000 build); the
rarely used classes are separate files in `LIBS:OpenMUI`.

## Speed

Milliseconds per operation, median of 9 repetitions (A600: 5), the same test
program on every library, cycle-exact FS-UAE emulation of an A1200 (68EC020,
14 MHz) and an A600 (68000, 7 MHz), OS 3.1 with 4 MB fast RAM. Lower is
better. Ranges are the spread over several instances of the emulated machine.

| Operation | A1200 OpenMUI | A1200 MUI 3.8 | A600 OpenMUI | A600 MUI 3.8 |
|---|---:|---:|---:|---:|
| Application, empty | 16 | 92-93 | 66-67 | 370 |
| Application with windows, create | 183-189 | 235-236 | 758 | 948 |
| Window open + close | 398 | 520-523 | 1,384-1,396 | 1,779 |
| Full redraw | 133 | 140-151 | 442 | 472 |
| Relayout | 173 | 197-207 | 613 | 701 |
| Slider set | 2.7 | 4.0-4.1 | 10.2 | 15.8 |
| Notification chain | 5.7 | 7.8-7.9 | 21.0 | 30.6 |
| Cycle set | 2.0 | 2.4 | 7.9 | 9.4 |
| Radio set | 8.3 | 15.6 | 32.4 | 60.7 |
| Text set | 2.4 | 2.5 | 9.4 | 9.7 |
| List insert, 100 rows | 61-62 | 830-833 | 231-232 | 2,760 |
| List insert, 1,000 rows | 248-249 | 6,652-6,662 | 1,014 | 24,027 |
| List sort, 1,000 rows | 95 | 659 | 384-386 | 2,504 |
| List scroll, one row | 13.2 | 14.6 | 37.4 | 40.0 |
| Button click | 16.6-16.8 | 22.3-22.4 | 66-69 | 91.5 |
| String, one key | 6.7 | 7.5-7.9 | 26.2-26.4 | 30.7-31.5 |
| String, backspace | 6.6 | 7.5-7.8 | 25.8-26.2 | 30.4-31.2 |
| Register page | 42.5 | 53.7-53.8 | 145 | 164 |
| Popup open + close | 95-96 | 120 | 341-347 | 416 |
| Uniconify | 337-340 | 450-451 | 1,248 | 1,556 |

Operations not listed (menu, iconify, window resize, list active, list scroll
by page, list insert of one row) are within 5 percent of MUI 3.8's or faster
on the A1200. In the A600 column, list insert of one row (33 ms against 32.5)
is level with MUI 3.8. The complete tables are in
[docs/lean/measurements.md](docs/lean/measurements.md).

Against MUI 5 (A1200, OS 3.2.1, OpenMUI opened as MUI 5 version 21.227), OpenMUI
is faster in all 27 operations measured:

| Operation | OpenMUI | MUI 5 | Ratio |
|---|---:|---:|---:|
| Application with windows, create | 236-250 | 578-587 | 0.40-0.43 |
| Window open + close | 481-482 | 1,027-1,041 | 0.46-0.47 |
| Full redraw | 157 | 206 | 0.76 |
| Cycle set | 2.4 | 2.7 | 0.87 |
| Radio set | 9.7 | 11.9 | 0.82 |
| List insert, 1,000 rows | 477 | 20,253-20,270 | 0.02 |
| Button click | 17.7 | 21.2 | 0.83 |
| String, one key | 7.6 | 15.4 | 0.49 |
| Uniconify | 422 | 1,221-1,236 | 0.34 |
| Popup open + close | 120 | 531-540 | 0.22 |

The MUI 5 comparison was measured on the build of 2026-11-12.

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

Tested in emulators against real MUI 3.8 and MUI 5 with 184 programs and class
demos (100 MUI 3 programs, 24 MUI 4/5 programs, 60 add-on class demos),
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
