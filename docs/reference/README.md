# OpenMUI programmer reference

The reference for programs and classes written for OpenMUI's `muimaster.library`. The
[developer guide](../developer-guide.md) is the overview (what is supported, how the identity works,
building, tests, the clean-room rule); this is the detail. Content is derived from OpenMUI's sources
and from black-box measurement of MUI; nothing has been run on a real Amiga. OpenMUI is not MUI and is
not affiliated with MUI's authors. None of MUI's code is used.

## Pages

| Page | What |
|---|---|
| [functions.md](functions.md) | the `muimaster.library` functions: vector offsets, registers, arguments, results, notes |
| [classes-core.md](classes-core.md) | Notify, Family, Application, Window, Area, Rectangle, Semaphore, Dataspace, Configdata, Menuitem/Menu/Menustrip, Boopsi |
| [classes-layout.md](classes-layout.md) | Group, Virtgroup, Scrollgroup, Register, Balance, Selectgroup, Scrollbar, Scrollbutton |
| [classes-display.md](classes-display.md) | Text, Image, Bitmap, Bodychunk, ChunkyImage, Dtpic, Imagedisplay, Framedisplay, Pendisplay, Gauge, Scale, Colorfield, Levelmeter, Crawling |
| [classes-input.md](classes-input.md) | String, Prop, Numeric, Slider, Numericbutton, Knob, Cycle, Radio, Palette |
| [classes-lists.md](classes-lists.md) | List, Listview, Floattext, Volumelist, Dirlist |
| [classes-popups.md](classes-popups.md) | Popstring, Popobject, Poplist, Popasl, Popscreen, Poppen, Popimage, Popframe |
| [classes-mui45.md](classes-mui45.md) | classes MUI 4/5 added: Title, Panel (requester panels), Fontpanel, Fontdisplay, Filepanel, Keyadjust, Datamap, Objectmap, Textdata, Process |
| [classes-prefs.md](classes-prefs.md) | Settingsgroup, Settings, Aboutmui, Mccprefs and the adjust classes |
| [custom-classes.md](custom-classes.md) | writing a custom class and an external `.mcc`; a compiling example |
| [differences.md](differences.md) | where OpenMUI differs from MUI 3.8 and MUI 5, dated |

The `functions.md` and `classes-*.md` pages are **generated** by `tools/gen_reference.py` from the
headers (`workbench/libs/muimaster/classes/*.h`, `mui.h`, `include/fd/muimaster_lib.sfd`), the class
dispatchers and `OM_NEW`/`OM_SET`/`OM_GET` code, and the autodoc blocks in the sources. To correct a
sentence, correct the autodoc in the class source and run `python3 tools/gen_reference.py`. The
pages `README.md`, `custom-classes.md` and `differences.md` are written by hand.

Counts: 75 classes documented (and one, Scrmodelist, declared but not built),
524 attributes, 238 methods and 29 library functions.

## How OpenMUI relates to MUI's interface

- **The base is MUI 3.8's interface**: the class names (`MUIC_*`), attribute and method identifiers
  (`MUIA_*`, `MUIM_*`, `MUIV_*`), the numeric values and the argument structures (`MUIP_*`) are MUI's, so
  a program or class compiled against MUI's own headers works. The goal is MUI 3.8's behaviour at default
  settings (the developer guide, "What OpenMUI supports").
- **MUI 4 and MUI 5 additions** are present as classes loaded on demand, attributes and methods, and
  behaviour behind the per-program identity (developer guide, "MUI 4 and MUI 5", "How identity decides").
  A MUI 3 program never pays for them. The class pages mark attributes the header gives no version
  for (the MUI 4/5 ones) and the [differences](differences.md) page lists what is only active for
  MUI 4/5 identities.
- **OpenMUI additions.** Attribute and method names built on `MUIB_ZUNE` (for example
  `MUIA_Gauge_DupInfoText`) are Zune's own extensions inherited from the code OpenMUI started from; they
  are marked `Zune` in the "Since" column. A program that wants to run on real MUI should not use them.
- **Not provided:** `muiscreen.library` (its headers `libraries/muiscreen.h`,
  `clib/muiscreen_protos.h` and the `.sfd` are in the SDK, but there is no implementation in the source tree); the private vectors `MUI_Priv1` to `MUI_Priv6`; MUI's private structures beyond what
  programs read through MUI's own macros (see [custom classes](custom-classes.md#the-public-area-data)).

## Where the headers are

The release archive carries them in `SDK/include` (staged by `tools/components.py sdk`):

| Path in `SDK/include` | Content |
|---|---|
| `libraries/mui.h` | the one MUI header: made by `buildincludes` from `mui.h` and `classes/*.h`; every class identifier, attribute, method, message structure, `MUIM_*`/`MUIV_*` value, the object macros (`ApplicationObject`, `VGroup`, `SimpleButton`, `Child`, `End` ...) and the area macros (`_left`, `_mleft`, `_rp` ...) |
| `libraries/muiscreen.h`, `clib/`, `proto/`, `pragmas/`, `fd/`, `inline/` | the library's prototypes, `.fd`/`.sfd` files and inline calls for `muimaster.library` and `muiscreen.library` |
| `mui/` | the bundled add-on classes' headers (`TheBar_mcc.h`, `NList_mcc.h`, `TextEditor_mcc.h`, `BetterString_mcc.h`, `Listtree_mcc.h`, `Busy_mcc.h`, `Hyperlink_mcc.h`, `Rawimage_mcc.h` ...) and Spot's UP-MCC headers |
| `zune/` | headers of the Zune-originated classes (`aboutwindow.h`, `calendar.h`, `clock.h`, `flexgroup.h`, `graph.h`, `iconimage.h`, `loginwindow.h`, `prefseditor.h`, `prefswindow.h`, `qrcode.h`, `systemprefswindow.h`) |

`SDK/examples` has the example sources (the step-by-step tutorial programs, `TheBarDemo.c` and
`CustomClass.c`, the custom-class example); the archive's `Examples` drawer has the built programs of the
tutorial set. `Docs` has the add-on classes' own documents.

## Opening the library

Open it by name, `muimaster.library`, with the version your program needs:

```c
struct Library *MUIMasterBase = OpenLibrary("muimaster.library", 19);   /* MUIMASTER_VMIN/VLATEST */
```

There is no second library name. The version you ask for **decides
which MUI the program sees**:

| You ask for | Under "Behave as: Automatic" the program gets | `lib_Version.lib_Revision` it sees |
|---|---|---|
| 0 to 19 | MUI 3.8 | 19.35 |
| 20 | MUI 4 | 20.6000 (not a real MUI 4 number: above every MUI 4 revision the open class sources test) |
| 21 and up | MUI 5 | 21.227 |

The identity is the first match of the program's own line in `ENV:OpenMUI/BehaveAs`, the "Behave as"
setting for all programs unless it is Automatic, then Automatic (the table above); it is decided at the
program's first open and kept while it has the library open. The base in the library list reports MUI 5
(21.227) so that exec lets every program in; `Open` hands out a base reporting the program's identity
(source: `identity.c`, `native_version.h`; checked with `tests/lean/idtest.c`, see the developer guide).
So: a program that asks for 19 gets MUI 3.8 geometry and behaviour, and a program that wants MUI 5
behaviour asks for 21. An external class (`.mcc`) opens `muimaster.library` itself; OpenMUI gives it the
identity of the program that loads it (see [custom classes](custom-classes.md#external-classes-mcc)).

`MUIMASTER_VMIN` and `MUIMASTER_VLATEST` in `libraries/mui.h` are 0 on the OpenMUI build of the header
(`__AROS__` not defined); MUI's own header uses 11 and 19.

## Compiling a program

The repository's examples and tests use the GCC cross compiler (`m68k-amigaos-gcc`, GCC 16 used), as
the developer guide describes; there are no vbcc or SAS/C project files or notes in the repository, so
none are given here.

```sh
m68k-amigaos-gcc -noixemul -Os -ISDK/include -o HelloZune HelloZune.c -lmui
```

- `-lmui` is the link library with the variable-argument forms (`MUI_NewObject`, `MUI_Request`,
  `MUI_MakeObject`, `MUI_AllocAslRequestTags`, `MUI_AslRequestTags`). Defining `MUIMASTER_YES_INLINE_STDARG`
  makes the inline header provide them instead.
- Include `<proto/muimaster.h>` and define `struct Library *MUIMasterBase;` (as the examples do) when you open the library yourself.
- A program that opens the library by hand and draws needs a stack of its own choice
  (`ULONG __stack = 65536;` in `tests/native/disablepaint.c`).
- `CustomClass.c` in `SDK/examples` is built exactly like the others
  (`m68k-amigaos-gcc -noixemul -Os -I<SDK/include> -o CustomClass CustomClass.c -lmui`); it compiles without
  warnings with `-Wall` against the headers a build generates. It has not been run.

## Conventions on the class pages

Each class has a table of its identifier, superclass, where it lives (built into the library or a class
file `LIBS:OpenMUI/<Name>.mui`, which loads the first time a program makes one), an
attribute table (ISG flags, type, meaning, default), and a method table (the message structure after
`MethodID`, whether the class's own dispatcher handles it). Legend and its limits are at the top of every
class page. Meaning and defaults come from the autodoc comments in the sources: where a MUI 4/5 attribute
has none, the meaning column is filled from the code by hand or left empty.
