# OpenMUI user guide

OpenMUI is a MUI-compatible GUI system for AmigaOS 3.x, an alternative to MUI
(the Magic User Interface): `muimaster.library`, its classes, the add-on
classes programs use and its own preferences program. Programs written for
MUI open their windows through it. It runs programs
written for MUI 3, 4 and 5, and aims at the look and the memory use of MUI 3.8.

> OpenMUI is not MUI and is not affiliated with MUI's authors. MUI was created by Stefan Stuntz;
> MUI 4 and 5 come from the MUI team. OpenMUI is a separate library that is compatible
> with MUI's programming interface. It is built on Zune, the MUI-compatible
> toolkit of AROS (The AROS Development Team), as ported to AmigaOS 3 by
> Stefan Reinauer (Zune68).

**This is a beta (0.1b1).** It has been tested in emulators, not yet on a
real Amiga. Please report what works and what does not (see
[Reporting a problem](#reporting-a-problem)).

## What you need

- AmigaOS 3.1 or newer.
- A 68000 or better. No FPU is needed.
- Amiga Installer 43 or newer. OS 3.2 has it as `SYS:System/Installer`. On
  OS 3.1 use Installer 43.3 from Aminet (`util/misc`); the Installer on the
  OS 3.1 Install disk is older and the install script stops with a message.

## Installing

1. Unpack the archive `OpenMUI-0.1b1-YYYY.MM.DD-amigaos3-m68k.lha`.
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
- an earlier OpenMUI (or "Zune68k lean") installation.

Then it installs:

| From the archive | Goes to |
|---|---|
| `Libs/muimaster.library` | the first `LIBS:` drawer (normally `SYS:Libs`) |
| `Libs/Zune/*` | `LIBS:Zune` (OpenMUI's own class files and modules) |
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
- **A real MUI's own classes** (in `MUI:Libs/mui`) belong to that MUI. OpenMUI
  does not use them and does not touch them. It uses its own copies.
- **Your settings are kept.** MUI's settings (`ENVARC:MUI`) are not read or
  changed by OpenMUI, except when you ask for an import (see
  [Preferences](#the-preferences-program)). On an update, OpenMUI's own
  settings (`ENVARC:Zune`) are kept.

### Updating

Run *Install* again. It offers "Install this version over it": the earlier
installation is removed first (your own files are put back), then the new one
is installed, so an update always starts from your own files. An earlier test
build installed as "Zune68k lean" (`SYS:Zune68k`, `SYS:Prefs/Zune`) is
removed the same way first. A second library `zunemaster.library` that
earlier test builds installed is taken away: OpenMUI is one library now. If
one is still there after that and it is your own, it is moved to the backup
and put back by *Uninstall* and *Use real MUI*.

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
remove `ENV:Zune` and `ENVARC:Zune` too), `QUIET` and `HOME=<drawer>`.

## Uninstalling

Use the *Uninstall* icon in `SYS:OpenMUI`, or run *Install* again and choose
"Remove OpenMUI", or `OpenMUISwitch UNINSTALL` from a Shell.

It deletes every file OpenMUI installed, puts back every file of yours it had
replaced, and removes the drawers it made when they are empty. It asks
whether to keep your OpenMUI settings (`ENVARC:Zune`). Your MUI keeps working.
From Workbench the `SYS:OpenMUI` drawer itself is deleted a little later by a
script, because a Workbench window of the drawer locks it.

## The preferences program

`SYS:Prefs/OpenMUI` is OpenMUI's own settings program. It has the pages
System, Windows, Groups, Buttons, Cycles, Sliders, Scrollbars, Listviews,
Strings, Navigation, Special and Frames, and the settings pages of the
bundled classes that have one. The buttons at the bottom are Save, Use, Test,
Revert and Cancel. *Save* keeps the settings in `ENVARC:Zune` (and `ENV:Zune`),
*Use* only in `ENV:Zune` until the next reboot.

The Project menu has *Import MUI 3.8 settings...*. It reads MUI 3.8's files
in `ENVARC:MUI` (only reads them) and copies each program's own file into
OpenMUI's settings, unless OpenMUI already has one for that program. MUI 3.8's
global settings are shown in the editor; *Save* or *Use* keeps them,
*Cancel* goes back.

Settings that OpenMUI keeps in `ENV:Zune` and `ENVARC:Zune` are separate from
MUI's (`ENV:MUI`, `ENVARC:MUI`). The names `Zune` in these paths and in
`LIBS:Zune` are not renamed yet, because programs and earlier installations
depend on them.

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

The setting is a text file, `ENV:Zune/BehaveAs` (saved copy:
`ENVARC:Zune/BehaveAs`). You can edit it by hand.

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

## The files in LIBS:Zune

About 45 rarely used classes and parts of the library are files of their own
in `LIBS:Zune`, and are loaded the first time a program needs one. This keeps
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
| A feature is missing (no context menu, no requester) | Check that `LIBS:Zune` is complete: run *Install* again ("Install this version over it"). |
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
- your `ENV:Zune/BehaveAs`, if you changed it.

"Works" reports are welcome too: which program, which version.

## Licence

The core is under the AROS Public License (`LICENSE`). The bundled classes keep
their own licences and notices (`LEGAL`, `ACKNOWLEDGEMENTS`).
