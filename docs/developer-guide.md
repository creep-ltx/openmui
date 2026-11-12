# OpenMUI developer guide

For people who write programs for OpenMUI, or work on OpenMUI itself.
Users: see [user-guide.md](user-guide.md).

OpenMUI is not MUI and is not affiliated with MUI's authors. MUI was created by Stefan Stuntz;
MUI 4 and 5 come from the MUI team. OpenMUI is a separate library that is compatible
with MUI's programming interface.

## What OpenMUI supports

OpenMUI is one library, `muimaster.library`, plus class files in
`LIBS:Zune` and bundled add-on classes in `LIBS:MUI`. It implements MUI's
public programming interface: the built-in classes (Notify, Family,
Application, Window, Area, Rectangle, Group, Image, Text, String, List,
Listview, Numeric, Slider, Prop, Scrollbar, Cycle, Radio, Popstring and the
Pop classes, Register, Scrollgroup, Gauge, Palette, the adjust and display
classes, Dirlist and others), the `MUI_*` functions, custom classes
(`MUI_CreateCustomClass`) and external classes (`.mcc`, `.mcp`).

The goal is MUI 3.8: the same windows, sizes and behaviour at default
settings. The version it reports is 19.35 to MUI 3 programs.

### MUI 4 and MUI 5

MUI 4/5 programs run as fully as possible, but a MUI 3 program pays nothing
for it: every MUI 4/5-only piece is a file loaded on demand, or lies behind
the per-program identity (below). What is in, as read from the sources and
notes (`docs/lean/README.md`, "MUI 4/5 programs" and the rounds after it):

| Area | What |
|---|---|
| Classes (as files in `LIBS:Zune`) | Fontpanel (with Fontdisplay), Filepanel, Selectorpanel, Keyadjust, Title (with MUI 5's register tab look), Datamap (with Objectmap), Textdata, Selectgroup, Process |
| Classes (add-on files) | Hyperlink (`LIBS:MUI/Hyperlink.mcc`; about:, amigados:, workbench: in the class, other URIs through `openurl.library`). Pixmap (`LIBS:Zune/Pixmap.mui`) and Rawimage (`Rawimage.mcc`) with MUI 5's zlib-compressed ("Z") images |
| Attributes and methods | `MUIM_FindObject`, `MUIA_FrameVisible`, `MUIA_FrameDynamic` (the second is kept, not acted on), List's sort mark on a sortable column title, MUI 5 column attributes of List, MUI 5's scrollable contents for a window larger than the screen (only for programs that get MUI 5), `MUIA_Text_Shorten` elided texts, drawn pointer shapes, RGB text pens |
| Settings pages | Listtree and Busy (MUI 3.8's items) |

Known gaps are in [Known differences](#known-differences-from-mui).

### How identity decides

Programs and classes choose code paths by the version the library base
reports (TheBar sizes its virtual bar itself only below MUI 4, NList and
TextEditor branch on MUI 4 revisions). One number for the whole machine
would be wrong for somebody, so each program sees the MUI it asked for:

- exec's `OpenLibrary()` compares the version asked for with the library's
  `lib_Version` before it calls the library's Open. So the base in the
  library list reports MUI 5 (21.227) and lets every program in, and Open
  hands out a base for the program: the base itself for MUI 5, or a copy
  (the jump table and the Library structure, 342 bytes, shared by every
  program of that identity) reporting 19.35 (MUI 3.8) or 20.6000 (MUI 4).
  A call through a copy costs nothing more; Close and Expunge reach the
  shared base.
- Exec 40.10 (OS 3.1) and 47.8 (OS 3.2) pass the version asked for in D2 (the
  base is in D0). Open takes D0 when it is not the base, else D2.
- The identity is the first match of: the program's line in the list
  (`ENV:Zune/BehaveAs`), the "Behave as" setting for all programs unless
  Automatic, then Automatic (0 to 19 gives MUI 3.8, 20 MUI 4, 21 and up
  MUI 5). The file format is in the [user guide](user-guide.md#the-file).
- It is decided at the program's first open and remembered for the task
  while it has the library open (32 records), so the classes it opens from
  its own task and its application object follow it.
- External classes open `muimaster.library` in their init, which ramlib runs
  in its own task. OpenMUI sets the identity of the program that loads the
  class around the load.

Code in `workbench/libs/muimaster/identity.c`. `tests/lean/idtest.c` opens
the library with a version and prints what the base and the application
object report.

If you write a program or a class for OpenMUI and need MUI 5 behaviour, ask
for version 21. A program that asks for 19 gets MUI 3.8 under Automatic.

## Known differences from MUI

From "Left open" in `docs/lean/README.md`, the roadmap and the identity notes.
Emulator results only; nothing here is measured on a real Amiga.

**Compatibility**

- The group spare-pixel rule: a column case and one row case differ from MUI
  3.8's by a pixel or more in some groups.
- The window sizes of a few programs differ by pixels from MUI 3.8's (NFS-GUI's
  list by 1 pixel, TinyMeterPrefs' default height, and others listed in the
  README).
- Busy's MUI 5 look (grey with blue blocks) is not done.
- Popscreen sets no `WindowHook` (MUI 3.8 does).
- Voyager 3.5.3 can reset the machine after a quit by its close gadget. The
  write is Voyager's own blit, still running when its own `FreeBitMap`
  frees the bitmap. Decided as Voyager's bug.
- AmigaGPT keeps MUI 3.8's geometry, because it asks for version 19.
- RegisterGadget's popup menu and FrameDynamic's hover (MUI 4/5) are not done.

**Identity**

- A class library serves every program of the identity it was loaded for,
  while any program uses it.
- Two programs loading classes at the same moment can swap the loading
  identity (there is no lock; a class whose init loads another class through
  ramlib would deadlock on one).
- `SetFunction()` on `muimaster.library` patches the listed base. Programs
  holding a copy call the original functions.
- More than 32 programs with the library open: the others get no record.
- It relies on exec 40/47 keeping the version in D2. Another exec that passes
  neither D0 nor D2 would make every program MUI 3.8 under Automatic (the
  per-program list still works).

**Size and speed**

- The library file is about 256 KB on disk against MUI 3.8's 166 KB (emulator build
  figures, see `docs/lean/README.md`). What is left is mostly OpenMUI's own
  code: List, Window, String, Group, Area, Application.
- Creating an application with its windows is 1.06 to 1.08 times MUI 3.8's
  (FS-UAE, `opbench`). MUI 3.8 pools its objects; a pool here cost the
  first program 3 to 7 KB.
- Real-hardware speed has not been measured.

**Tested only in emulators** (Amiberry headless, FS-UAE). RTG only as
`uaegfx` with Picasso96 in FS-UAE. InstallerNG cannot be tested on Amiberry.

## Building from source

Needed: `m68k-amigaos-gcc` (GCC 16 used), GNU make, `sfdc`, `fd2sfd`,
FlexCat and `lha`; Python 3 for the checks.

```sh
git submodule update --init --recursive   # TheBar (the translations are in the repository)
make -j4 all
make check      # needs the Python bundled with the Amiga toolchain (amitools)
make release    # the archive and its Aminet readme, in build/muimaster/release
```

Sub-targets: `core` (the library), `prefs`, `plugins` (bundled classes),
`catalogs`, `sdk`, `examples`, `check-core`, `check-sdk`, `clean`.

`make check` runs the linked 68000 code with modelled OS calls in
amitools/vamos. It does not establish GUI compatibility or speed on Amiga
hardware.

Build options (from `docs/lean/`): the 68000 build is the default; a 68020
build is possible. `CLASSFILES=` (empty) builds every class into the library
instead of as files; `make check` also runs on that build. `LTO=1` builds
but stays off: it inlines methods into their dispatchers, which the stack
rule below forbids (`docs/lean/compiler.md`, "LTO, done right").

### Writing a program

Add `SDK/include` from the archive to the include path and link with `-lmui`:

```sh
m68k-amigaos-gcc -noixemul -Os -I../include -o HelloZune HelloZune.c -lmui
```

The archive has an `Examples` drawer (HelloZune, HGroup, VGroup, VHGroup,
VHGroup2, HVGroup, Notify, TheBarDemo). Open `muimaster.library` by that name
(the second name `zunemaster.library` is gone).

## The test tools

All in the repository. The ones that run Amiga programs need test machines
(emulator setups) that are not part of the repository.

| Tool | What it checks |
|---|---|
| `make check` (`tests/check_*.py`) | The library's code in a 68000 harness: classes, notifications, layout, images, the installer script, release naming, the settings importer, the parsers (fuzzed). In CI on every push. |
| `tests/lean/scripts/fullcheck.sh` | MUI 3.8's 19 demos and MUI-Demo's sub-windows open and close, no alert, memory given back. |
| `tests/lean/scripts/demopix.sh` | The demos' windows pixel by pixel against MUI 3.8. |
| `tests/lean/scripts/suite.sh` | The real-program suite: real MUI programs and add-on class demos started on OpenMUI and on real MUI (3.8 and 5) and compared. Table in `tests/lean/real-programs.md`. |
| `tests/lean/scripts/measure.sh`, `tests/lean/memtally/` | Memory per program. |
| `tests/lean/scripts/opbench.sh`, `tests/native/opbench.c` | Time of common operations against MUI 3.8. |
| `tests/lean/memtally/failn` with `failsweep.sh`, `failreal.sh` | Out-of-memory: the Nth resource call from library code fails. |
| `soak.sh`, `smallcheck.sh`, `prefsfuzz.sh`, `rexxfuzz.sh` | Hours of use, a stock 68000 with 0 to 2 MB fast RAM, damaged settings files, ARexx commands. |

The method and results of each round are in `docs/lean/README.md`;
[docs/production.md](production.md) lists the gates for 1.0.

## The clean-room rule

- **MUI is studied black-box only.** Its behaviour is measured (what a MUI
  program does on real MUI: windows, sizes, timing, the files it writes), and
  its public documentation is read: the autodocs, the SDK headers and the
  guide that come with it. MUI's code is never read or disassembled and
  never copied.
- **Third-party programs and classes may be disassembled for
  interoperability**, to see what they expect of the library. An example in
  the notes is the size method of the image classes that ship with MUI 5,
  seen in the dispatcher of a third-party image class. That is the
  third party's code, not MUI's.
- A commit message says how a behaviour was found (probe name, side by side
  on MUI 3.8 or MUI 5).
- The AROS-derived core is under the AROS Public License; bundled classes
  keep their own licences (`LEGAL`, `ACKNOWLEDGEMENTS`).

## Contributing

Issues and pull requests go to <https://github.com/creep-ltx/openmui>.

- Say what you changed and how you checked it. A found bug gets a probe or a
  check where it can.
- Keep to the rules the project uses: one change per commit; `make check`
  passes; a MUI 3 program must pay nothing for MUI 4/5 (the code goes into a
  class file or module, or behind the identity); stack frames do not grow
  (compare `-fstack-usage`) and methods are not inlined into dispatchers;
  every allocation result is checked and a failed object creation leaves
  nothing behind; MUI is studied black-box.
- Put no private paths, keys or e-mail addresses in the repository.
- Betas are `X.YbN`. Every change gets an entry in [../CHANGELOG.md](../CHANGELOG.md).

Where the work stands, what is next and the open items:
[lean/roadmap.md](lean/roadmap.md), `docs/lean/README.md` ("Left open").
