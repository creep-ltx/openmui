# OpenMUI

A free, open-source alternative to MUI for classic AmigaOS 3.x. Install it
instead of MUI, and programs made for MUI 3, 4 and 5 work with it. It comes
with everything MUI programs need: `muimaster.library`, the classes, and a
preferences program. It runs on a stock 68000 with OS 3.1, and looks and
uses memory like MUI 3.8.

## OpenMUI in numbers

Measured in an emulated Amiga 1200 (68EC020, FS-UAE, cycle-exact; MUI 5 on OS 3.2.1, the others on
OS 3.1), 2026-11-12. Not yet measured on a real Amiga.

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

All figures, with exact values on hover and table views:
[docs/stats/openmui-stats.html](docs/stats/openmui-stats.html) (download and open it in a browser).
How each was measured: [docs/lean/measurements.md](docs/lean/measurements.md).

**Version 0.1b1, a test build.** Betas are `X.YbN` until OpenMUI has been in
daily use for a while. Every figure in these pages was measured in an emulator
(FS-UAE), not on a real Amiga.

> **OpenMUI is not MUI and is not affiliated with MUI's authors.** MUI was created by Stefan Stuntz;
> MUI 4 and 5 come from the MUI team. OpenMUI is a separate library that is compatible
> with MUI's programming interface. MUI 3.8 was studied clean-room: its
> behaviour, never its code.

## Where it comes from

| | |
|---|---|
| **Zune** | The MUI-compatible GUI toolkit of [AROS](https://github.com/aros-development-team/AROS), by The AROS Development Team. |
| **Zune68** | Zune ported to classic AmigaOS 3 by Stefan Reinauer ([reinauer/zune68](https://github.com/reinauer/zune68)). |
| **OpenMUI** | The *lean edition* of Zune68: MUI 3.8's look, memory use and speed as the goal, MUI 3/4/5 programs as the target. Earlier test builds were called Zune68k lean. |

The AROS Public License applies to the core (see `LICENSE`); the bundled
classes keep their own licences and notices (`LEGAL`, `ACKNOWLEDGEMENTS`).

## What is in it

- **One library, muimaster.library**, built with GCC for 68000 or better, no
  FPU. (Earlier test builds also installed it a second time as
  `zunemaster.library` for the preferences program; the installer now takes
  that copy away.)
- **Each program gets the MUI it asks for.** A MUI 3 program sees MUI 3.8
  (19.35), a MUI 4 program MUI 4, a MUI 5 program MUI 5 (21.227). OpenMUI prefs
  has "Behave as" (Automatic, MUI 3.8, MUI 4, MUI 5) and a list of programs
  with a MUI of their own.
- **Lean:** about 45 rarely used classes and parts of the library are files of
  their own in `LIBS:OpenMUI`, loaded the first time a program needs one.
- **OpenMUI preferences** (`SYS:Prefs/OpenMUI`), with an import of MUI 3.8's
  settings.
- **Bundled MUI classes** with their preference pages: NList, NListtree,
  TextEditor, BetterString, TheBar, Busy, Listtree and others.
- **Installer** that leaves a real MUI alone, keeps what it replaces,
  and has *Use real MUI*, *Use OpenMUI* and *Uninstall* in `SYS:OpenMUI`.
- Translations, an SDK with examples, and the full source.

## Install

Download the archive `OpenMUI-0.1b1-YYYY.MM.DD-amigaos3-m68k.lha`, unpack it
and double-click *Install* (Installer 43 or newer; on OS 3.1 use Installer
43.3 from Aminet). The `ReadMe` inside has the details. An earlier Zune68k lean
test build is removed first and your own files put back.

OpenMUI's own drawers are `LIBS:OpenMUI` (its classes) and `ENVARC:OpenMUI` /
`ENV:OpenMUI` (its settings). Earlier test builds used `LIBS:Zune`,
`ENVARC:Zune` and `ENV:Zune`: installing over one moves the settings to the
new names and takes its files out of `LIBS:Zune`. The library file
`muimaster.library` and its version numbers keep their names: programs
depend on them.

## Build

With m68k-amigaos-gcc (GCC 16 used), GNU make, sfdc, fd2sfd, FlexCat and lha:

```sh
git submodule update --init --recursive
make -j4 all
make check     # needs the Python bundled with the Amiga toolchain
make release   # the archive and its Aminet readme, in build/muimaster/release
```

`make check` runs the linked 68000 code with modelled OS calls; it does not
establish GUI compatibility or speed on Amiga hardware.

## Guides

- [User guide](docs/user-guide.md): install, switch back to a real MUI, uninstall, the preferences,
  "Behave as", troubleshooting.
- [Developer guide](docs/developer-guide.md): what is supported, known differences from MUI,
  building, tests, the clean-room rule, contributing.
- [Changelog](CHANGELOG.md).

## More

Details are in [docs/lean/](docs/lean/README.md): what differs from Zune68
commit by commit, memory and speed measurements, the class split, how each
program gets its MUI, the roadmap. [dist/Compatibility](dist/Compatibility)
describes the compatibility workloads; [tests/native/PERFORMANCE.md](tests/native/PERFORMANCE.md)
the speed method.

Tested with (in FS-UAE): MUI 3.8's 19 demos and real MUI programs such as YAM,
AmIRC, AmFTP and IBrowse; 30 of 36 MUI 3 programs behave as with MUI 3.8 there.

![IBrowse 3.0 running with Zune68, next to Zune Prefs](iBrowse.png)

*(Picture from the Zune68 base, before the OpenMUI name.)*

## Branches and the upstream import

- `aros-upstream` holds the filtered AROS history (component source, layout,
  authorship and dates retained; commit messages carry an `AROS-Commit`
  trailer). `main` is Zune68's work on top of it; `lean` is the lean edition's
  line, and OpenMUI is that line under its new name.
- The importer, the path list and how to merge later AROS updates are in
  `tools/upstream/` (`import-aros.py`, `paths.txt`, `source.json`,
  `commit-map`). Use a complete AROS clone and a new output directory; never
  fetch the unfiltered AROS branch into `aros-upstream`.
- TheBar is a git submodule (the translations are in the repository): run
  `git submodule update --init --recursive` after cloning.

GitHub Actions builds and checks pushes; a date tag such as `v2026.11.11`
creates a draft pre-release with the archive and Aminet readme.
