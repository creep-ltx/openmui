# OpenMUI user guide

OpenMUI is a free, open-source alternative to MUI (the Magic User
Interface) for AmigaOS 3.x. It provides `muimaster.library`, the built-in
classes and a preferences program, and runs programs written for MUI 3, 4 and
MUI 5. Its look and memory use follow MUI 3.8.

> OpenMUI is not MUI and is not affiliated with MUI's authors. MUI was created
> by Stefan Stuntz; MUI 4 and 5 come from the MUI team. OpenMUI is a separate
> library that is compatible with MUI's programming interface. It is based on
> Zune, the MUI-compatible toolkit of AROS (The AROS Development Team), as
> ported to AmigaOS 3 by Stefan Reinauer (Zune68).

**Version 1.0.** Problem reports: see
[Reporting a problem](#reporting-a-problem).

## Memory and speed

Cycle-exact FS-UAE emulation, OS 3.1, 4 MB fast RAM (MUI 5: OS 3.2.1),
measured 2026-11-23 (Zune68: Beta 5, 2026.11.22). Not measured on a real Amiga.

| | OpenMUI | MUI 3.8 | Zune68 | MUI 5 |
|---|---:|---:|---:|---:|
| MUI-Demo, first program | 427.3 KB | 460.3 KB | 672.3 KB | 1,293.7 KB |
| Each further program | 188.6 KB | 210.6-218.9 KB | 267.9 KB | 427-435 KB |
| Inserting 1,000 list rows, A1200 | 248 ms | 6,653 ms | 674-683 ms | 20,244-20,302 ms |

Further figures: [README](../README.md#speed).

## What you need

- AmigaOS 3.1 or newer.
- A 68000 or better. No FPU is needed.
- Amiga Installer 43 or newer. OS 3.2 has it as `SYS:System/Installer`. On
  OS 3.1 use Installer 43.3 from Aminet (`util/misc`); the Installer on the
  OS 3.1 Install disk is older and the install script stops with a message.

## Installing

1. Unpack the archive `OpenMUI-1.0-YYYY.MM.DD-amigaos3-m68k.lha (on Aminet: OpenMUI.lha)`.
2. Double-click *Install*, or from a Shell in that drawer:
   `Installer Install MINUSER AVERAGE`.
3. Reboot when it is done.

If Workbench cannot find the Installer, set the Default Tool of the Install
icon to its full path.

### What the installer does

First it looks, and changes nothing of what it finds:

- a MUI you already have (MUI 3.x, 4 or 5, in `MUI:` or `SYS:MUI`),
- MUI's lines in `S:User-Startup`,
- every `muimaster.library` in the `LIBS:` drawers,
- an earlier OpenMUI installation, including test builds named "Zune68k lean".

Then it installs:

| From the archive | Goes to |
|---|---|
| `Libs/muimaster.library` | the first `LIBS:` drawer (normally `SYS:Libs`) |
| `Libs/OpenMUI/*` | `LIBS:OpenMUI` (OpenMUI's own class files and modules) |
| `Libs/MUI/*` | `LIBS:MUI` of the first `LIBS:` drawer, never into a real MUI's own `MUI:Libs/mui` |
| `Prefs/OpenMUI` | `SYS:Prefs/OpenMUI` |
| `Locale/Catalogs/*` | `LOCALE:Catalogs/<language>` (optional) |
| `Tools/OpenMUISwitch` and the icons | `SYS:OpenMUI` |

### If you already have MUI

- **Your MUI is left alone.** OpenMUI goes into the first `LIBS:` drawer,
  which programs search before MUI's own `MUI:Libs`. So programs get OpenMUI
  once it is installed.
- **Nothing is added to `S:Startup-Sequence` or `S:User-Startup`.** OpenMUI
  needs no assign.
- **Every file it replaces is kept** in `SYS:OpenMUI/Data/Backup`, and every
  file it installs is written down in `SYS:OpenMUI/Data/Manifest`.
- **Bundled classes** (NList, NListtree, TextEditor, TheBar and others) are
  installed only when you do not have the same or a newer version in a
  `LIBS:MUI` drawer. An older one of yours goes to the backup.
  Updating from 1.0 leaves any UP-MCC files it installed in place (Uninstall
  leaves them too).
- **A real MUI's own classes** (in `MUI:Libs/mui`) belong to that MUI. OpenMUI
  does not use them and does not touch them. It uses its own copies.
- **Your settings are kept.** MUI's settings (`ENVARC:MUI`) are not read or
  changed by OpenMUI, except when you ask for an import (see
  [Preferences](#the-preferences-program)). On an update, OpenMUI's own
  settings (`ENVARC:OpenMUI`) are kept.

### Updating

Run *Install* again. It offers "Install this version over it": the earlier
installation is removed first (your own files are put back), then the new one
is installed, so an update always starts from your own files. A test build
installed as "Zune68k lean" (`SYS:Zune68k`, `SYS:Prefs/Zune`) is removed the
same way first. The second library `zunemaster.library` that test builds
installed is removed: OpenMUI is one library. If one remains and is not
OpenMUI's, it is moved to the backup and restored by *Uninstall* and *Use
real MUI*.

Test builds kept their class files in `LIBS:Zune` and their settings in
`ENVARC:Zune` and `ENV:Zune`. An update moves the settings to
`ENVARC:OpenMUI` and `ENV:OpenMUI` (a file already there is not replaced; the
old one then stays where it is), and takes the test build's own files out
of `LIBS:Zune`. Anything else in `LIBS:Zune` is yours: it goes to the backup
and comes back with *Uninstall* or *Use real MUI*.

Novice level installs everything without questions. Intermediate and Expert
ask whether to install the translations. Pretend mode changes nothing.

## Switching between OpenMUI and a real MUI

The drawer `SYS:OpenMUI` has these icons:

| Icon | What it does |
|---|---|
| *Use real MUI* | Puts OpenMUI's libraries and classes aside (into `SYS:OpenMUI/Data/Parked`) and puts your own files back. After a reboot programs get your MUI. Only there when a real MUI was found. |
| *Use OpenMUI* | The other way round. After a reboot programs get OpenMUI. Only there when a real MUI was found. |
| *Uninstall* | See below. |
| *ReadMe* | The text of the archive. |

*Use real MUI* checks first that a real MUI will be found afterwards. If a
move fails, the ones before it are undone.

The same from a Shell (the tool is `OpenMUISwitch`):

```
SYS:OpenMUI/Data/OpenMUISwitch MUI          ; use the real MUI
SYS:OpenMUI/Data/OpenMUISwitch OPENMUI      ; use OpenMUI
SYS:OpenMUI/Data/OpenMUISwitch UNINSTALL    ; remove OpenMUI
SYS:OpenMUI/Data/OpenMUISwitch STATUS       ; what is installed where
```

`UNINSTALL` and the other actions also take `SETTINGS` (with `UNINSTALL`:
remove `ENV:OpenMUI` and `ENVARC:OpenMUI` too, and an earlier test build's
`ENV:Zune` and `ENVARC:Zune`), `QUIET` and `HOME=<drawer>`.

## Uninstalling

Use the *Uninstall* icon in `SYS:OpenMUI`, or run *Install* again and choose
"Remove OpenMUI", or `OpenMUISwitch UNINSTALL` from a Shell.

It deletes every file OpenMUI installed, puts back every file of yours it had
replaced, and removes the drawers it made when they are empty. It asks
whether to keep your OpenMUI settings (`ENVARC:OpenMUI`). Your MUI keeps working.
From Workbench the `SYS:OpenMUI` drawer itself is deleted a little later by a
script, because a Workbench window of the drawer locks it.

## The preferences program

`SYS:Prefs/OpenMUI` is OpenMUI's own settings program. It has the pages
System, Windows, Groups, Buttons, Cycles, Sliders, Scrollbars, Listviews,
Strings, Navigation, Special and Frames, and the settings pages of the
bundled classes that have one. The buttons at the bottom are Save, Use, Test,
Revert and Cancel. *Save* keeps the settings in `ENVARC:OpenMUI` (and
`ENV:OpenMUI`), *Use* only in `ENV:OpenMUI` until the next reboot.

The Project menu has *Import MUI 3.8 settings...*. It reads MUI 3.8's files
in `ENVARC:MUI` (only reads them) and copies each program's own file into
OpenMUI's settings, unless OpenMUI already has one for that program. MUI 3.8's
global settings are shown in the editor; *Save* or *Use* keeps them,
*Cancel* goes back.

Settings that OpenMUI keeps in `ENV:OpenMUI` and `ENVARC:OpenMUI` are separate
from MUI's (`ENV:MUI`, `ENVARC:MUI`).

Test builds kept their settings in `ENVARC:Zune` and `ENV:Zune`. Installing
over such a build moves them to the new names. If they are still there
(for example after a manual copy), the library uses them until OpenMUI
has settings of its own, and the preferences program copies them over the
first time it runs; the old files are left as they are.

## "Behave as"

Programs and their classes choose how they work by the MUI version they find.
So OpenMUI gives each program the MUI it asks for. The library reports MUI 5
(21.227) so that every program can open it, and hands each program a view of
the version it needs.

On the System page of OpenMUI prefs, under "MUI version", you find **Behave as**
with four choices:

| Choice | Meaning |
|---|---|
| Automatic (the default) | By the version the program asks for: 0 to 19 gets MUI 3.8 (19.35), 20 gets MUI 4, 21 and higher gets MUI 5 (21.227). |
| MUI 3.8 | All programs get MUI 3.8. |
| MUI 4 | All programs get MUI 4. |
| MUI 5 | All programs get MUI 5. |

Below it is a list of programs with a MUI of their own. Type the program's
name in "Program:", choose MUI 3.8, MUI 4 or MUI 5 and press *Add*. *Remove*
takes the selected line away. **A line in the list wins over the setting for
all programs.**

Many programs written for MUI 4 or 5 ask only for version 19, and so get MUI
3.8 under Automatic. If such a program shows less than it should, give it
MUI 5 in the list.

The setting is used by programs started after you save or use it. A running
program keeps what it got.

### The file

The setting is a text file, `ENV:OpenMUI/BehaveAs` (saved copy:
`ENVARC:OpenMUI/BehaveAs`). You can edit it by hand.

- The first line is the setting for all programs: `Auto`, `MUI3.8`, `MUI4`
  or `MUI5`. (`MUI3` and `3.8` are accepted for MUI 3.8, "Automatic" for Auto.)
- Every further line gives one program a MUI of its own: `MUI3.8`, `MUI4` or
  `MUI5`, a space, then the program's name.

```
Auto
MUI5 YAM
MUI3.8 SomeOldProgram
```

The name is the program's file name, without path, and capital letters do not
matter. For a program started from a Shell it is the command name; from
Workbench it is the task name. Lines that cannot be read are ignored. The
file can be 2,047 bytes at most.

OpenMUI reads the file when the library is loaded, and the preferences
program makes the library read it again when you save or use. If you edit
the file by hand, run OpenMUI prefs and press *Use*, or reboot.

### Limits

- A class library that is in memory serves every program with the identity of
  the program it was loaded for, while any program uses it. A program of
  another identity gets it loaded again only when nobody uses it any more.
- Up to 32 programs with the library open at the same time keep their own
  identity. Further ones get the setting for all programs, else MUI 3.8.
- A class that a program opens itself with `OpenLibrary` (not through MUI)
  gets the identity by the version the class asks for.

## External images

The External tab of the image and frame pop-ups lists image sets, pictures
and image classes (`.image`, `.mim`, `.mi1`, `.mf0`, ...) from these drawers,
in this order:

1. `LIBS:OpenMUI/Images`
2. `LIBS:Zune/Images` (earlier OpenMUI and Zune68k test builds)
3. `MUI:Images` (a real MUI's images, only when `MUI:` is assigned)

The lists are merged; when a name is in more than one drawer, the first one
wins. Programs look in the same order when an image is not found by its full
name. The installer copies `LIBS:Zune/Images` to `LIBS:OpenMUI/Images` when
there is no such drawer (copied, not moved; Uninstall does not delete it).
OpenMUI ships no image sets: the sets in MUI's packages belong to their
authors and artists. Put your own into `LIBS:OpenMUI/Images`.

## The files in LIBS:OpenMUI

About 45 rarely used classes and parts of the library are files of their own
in `LIBS:OpenMUI`, and are loaded the first time a program needs one. This keeps
the library, and the memory a program takes, small. Class files are named
`<Name>.mui` (for example `Register.mui`, `Gauge.mui`, `Popasl.mui`). Parts of
the library that are not classes are `Z<name>.mui` (for example `Zmenu.mui`
for context menus, `Zrequest.mui` for requesters, `Zpicture.mui` for pictures
from datatypes).

- Do not delete or rename files there. If one is missing, only the feature
  that needs it fails (no context menu, no requester, no picture); the
  program goes on.
- A program that does not need a class never loads it, so you do not pay
  for it.
- The installer, `Use real MUI` and `Uninstall` look after these files.

## Troubleshooting

| Problem | What to try |
|---|---|
| The installer stops: "needs Amiga Installer 43" | Use Installer 43.3 (OS 3.1, Aminet `util/misc`) or the one of OS 3.2. |
| A program looks wrong or does not open a window | Try the other identity for it: add it to the "Behave as" list as MUI 3.8 or as MUI 5, save, start it again. |
| A MUI 4/5 program misses features | Give it MUI 5 in the list: it may ask only for version 19. |
| A change in "Behave as" does nothing | It holds for programs started after saving. Quit the program and start it again. |
| A feature is missing (no context menu, no requester) | Check that `LIBS:OpenMUI` is complete: run *Install* again ("Install this version over it"). |
| You want your real MUI back | *Use real MUI* in `SYS:OpenMUI`, then reboot. |
| You want OpenMUI gone | *Uninstall* in `SYS:OpenMUI`. |
| Something else is wrong | Check `OpenMUISwitch STATUS`, then report it. |

### Reporting a problem

Open an issue at <https://github.com/creep-ltx/openmui/issues>. Please say:

- the program and its version, and what you did,
- what you expected and what happened (a screenshot helps),
- the OpenMUI version (`Version LIBS:muimaster.library FULL`),
- your Amiga model, CPU, memory and OS version,
- whether the program works with your real MUI, if you have one,
- your `ENV:OpenMUI/BehaveAs`, if you changed it.

"Works" reports are welcome too: which program, which version.

## Licence

The core is under the AROS Public License (`LICENSE`). The bundled classes keep
their own licences and notices (`LEGAL`, `ACKNOWLEDGEMENTS`).
