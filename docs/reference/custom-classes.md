# Writing a custom class and an external class

Part of the [reference](README.md). Everything here was read from the sources (`mui_createcustomclass.c`,
`support_classes.c`, `classes/area.c`, `classes/area.h`, `macros.h`, `identity.c`) except where it says
*verified*. The example compiles; it has not been run.

## A class private to a program

```c
struct MUI_CustomClass *mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL,
    sizeof(struct Meter_DATA), (APTR) Meter_Dispatcher);
Object *o = NewObject(mcc->mcc_Class, NULL, MUIA_Frame, MUIV_Frame_Gauge, TAG_DONE);
...
MUI_DeleteCustomClass(mcc);          /* after every object of the class is disposed */
```

A complete, compiling program is [`CustomClass.c`](../../workbench/libs/muimaster/tutorial/examples/CustomClass.c)
(shipped in `SDK/examples`): a `Meter` subclass of Area with a private attribute, `OM_NEW`/`OM_SET`/`OM_GET`,
`MUIM_AskMinMax`, `MUIM_Setup`/`MUIM_Cleanup` (an event handler), `MUIM_HandleEvent` and `MUIM_Draw`, driven by a
Slider and a notification. Build (verified 2026-11-18, no warnings with `-Wall`):

```sh
PATH=~/opt/amiga-gcc16/bin:$PATH m68k-amigaos-gcc -noixemul -std=gnu17 -Os -Wall \
    -I<build>/include -Iworkbench/libs/muimaster/include -o CustomClass CustomClass.c -lmui
```

(`<build>/include` is where `make` puts the generated `libraries/mui.h` and `inline/muimaster.h`; an unpacked
release has them in `SDK/include`.)

### `MUI_CreateCustomClass` ([details](functions.md#mui_createcustomclass))

| Argument | Meaning |
|---|---|
| `base` | the class library's base for a class that lives in a library, else `NULL` (private class). With a base, its node name gives the class ID and the base is passed to the dispatcher through the dispatcher's data field. |
| `supername` | the superclass by name (`MUIC_Area`, ...), or `NULL`. **If given it wins over `supermcc`**, as on MUI 3.8 and MUI 5 (measured, `tests/lean/ccprio.c`). |
| `supermcc` | an existing custom class as superclass, used only when `supername` is `NULL`. |
| `datasize` | bytes of instance data your class adds; get them with `INST_DATA(cl, obj)`. |
| `dispatcher` | the dispatcher entry point: class in `a0`, object in `a2`, message in `a1`. |

The result is a `struct MUI_CustomClass *` (`mcc_Class` is the BOOPSI class) or `NULL`: neither superclass
supplied, the named superclass cannot be obtained, or no memory. Keep the dispatcher and the base valid
while the class exists. `MUI_DeleteCustomClass` returns FALSE, and the descriptor stays allocated, when `FreeClass()` refuses (instances or subclasses remain); dispose the objects first.

### The dispatcher

A dispatcher is an ordinary BOOPSI dispatcher. Handle the methods you care about and pass **everything else**
to the superclass with `DoSuperMethodA(cl, obj, msg)`. In GCC code the register arguments are written
with `__asm("a0")` and so on, as in the example. OpenMUI's own classes use a `BOOPSI_DISPATCHER` macro from its internal headers for the same thing.

## Method rules

The order and the rules below come from the Area autodocs (`classes/area.c`) and the code.

| Method | What your class does | Rules |
|---|---|---|
| `OM_NEW` | call the superclass first (`DoSuperMethodA` or `DoSuperNew`), read your tags with `GetTagData`, set up instance data | on failure after the superclass succeeded, dispose through `CoerceMethod(cl, obj, OM_DISPOSE)` (the built-in classes do) |
| `OM_SET` | take your attributes, then call the superclass | to suppress a notification for a tag that did not change, set `tag->ti_Tag = TAG_IGNORE` before calling the superclass (as the Gauge and the example do). Redraw with `MUI_Redraw(obj, MADF_DRAWUPDATE)` |
| `OM_GET` | answer your attributes by writing `*msg->opg_Storage` and returning TRUE; the superclass for the rest | |
| `MUIM_AskMinMax` | **call the superclass first**, then **add** your content size to `MinMaxInfo->MinWidth/MinHeight/DefWidth/DefHeight/MaxWidth/MaxHeight` | the superclass replaces `MinMaxInfo` with the frame and inner spacing contribution; use `MUI_MAXMAX` (10000) for "no limit". Fixed-size, text and maximum-size constraints are applied afterwards by the layout code, not by this method |
| `MUIM_Setup` | superclass first; if it fails return FALSE; then get your resources (event handlers, pens, fonts) | runs once per setup interval; the render environment (`_win`, `_dri`, `_screen`, `_pens`, `_font`) is valid from here to `MUIM_Cleanup`. **No geometry-dependent work**: the layout is not known yet |
| `MUIM_Show` | resources that depend on size and position | `MADF_CANDRAW` is set by the framework after a successful return; `_window`, `_rp` are valid from here to `MUIM_Hide` |
| `MUIM_Draw` | call the superclass, then draw your content if `msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)` | the superclass draws the background (when `MADF_DRAWOBJECT`) and the frame and title (when `MADF_DRAWFRAME`/`MADF_DRAWALL`, or for custom/rounded frames and titled objects) and writes the effective flags into the message; `MADF_DRAWUPDATE` alone leaves the background and frame to you. Start drawing with `MUI_Redraw()`, not by sending the method with a flag. Drawing may change the RastPort state; the clip rectangle may change too. The coordinates `_mleft` .. `_mbottom` are valid during `MUIM_Draw` |
| `MUIM_Hide` | release what `MUIM_Show` made | the framework clears `MADF_CANDRAW` before calling |
| `MUIM_Cleanup` | release what `MUIM_Setup` made (remove event handlers), then call the superclass | not idempotent: call once per setup; keep the window alive until cleanup is done |
| `MUIM_HandleEvent` | handle the IDCMP events you asked for | return `MUI_EventHandlerRC_Eat` to stop other handlers, 0 otherwise. Never reply to or keep `msg->imsg` (a temporary copy). Handlers can send notifications synchronously, so keep your object alive across them |
| `MUIM_HandleInput` | legacy handler for `MUI_RequestIDCMP`: Area does nothing; the Window ignores the return value | prefer the registered `MUIM_HandleEvent` model |

Register an event handler by filling a `struct MUI_EventHandlerNode` (`ehn_Priority`, `ehn_Flags`,
`ehn_Object`, `ehn_Class` for `CoerceMethod` or NULL for `DoMethod`, `ehn_Events` = IDCMP flags) and sending
`MUIM_Window_AddEventHandler` to `_win(obj)` in Setup, `MUIM_Window_RemEventHandler` in Cleanup.
`MUI_EHF_GUIMODE` in `ehn_Flags` means the handler is not called while the object is not visible or is
disabled. The older way, `MUI_RequestIDCMP(obj, flags)` / `MUI_RejectIDCMP`, delivers `MUIM_HandleInput` (OpenMUI
sets the private `MUI_EHF_HANDLEINPUT` flag for that).

## The area macros

Defined in `libraries/mui.h` (`macros.h` in the source). Use them; do not read the structure.

| Valid | Macros |
|---|---|
| between `MUIM_Setup` and `MUIM_Cleanup` | `_app`, `_win`, `_dri`, `_screen`, `_pens`, `_font` |
| during `MUIM_Draw` | `_left`, `_top`, `_width`, `_height`, `_right`, `_bottom` (the whole object including frame and inner spacing); `_addleft`, `_addtop`, `_subwidth`, `_subheight` (frame and inner spacing); `_mleft`, `_mtop`, `_mwidth`, `_mheight`, `_mright`, `_mbottom` (the area inside the frame, where you draw) |
| between `MUIM_Show` and `MUIM_Hide` | `_window`, `_rp`, `_minwidth`, `_minheight`, `_maxwidth`, `_maxheight`, `_defwidth`, `_defheight`, `_flags` |

`_pens(obj)[MPEN_*]` gives the pen numbers (`MPEN_SHINE`, `MPEN_HALFSHINE`, `MPEN_BACKGROUND`,
`MPEN_HALFSHADOW`, `MPEN_SHADOW`, `MPEN_TEXT`, `MPEN_FILL`, `MPEN_MARK`). Pens you obtain yourself with
`MUI_ObtainPen` are released with `MUI_ReleasePen` (see [functions](functions.md#mui_obtainpen)).

### The public area data

These macros read the object through a fixed layout. Programs and classes compiled against MUI's header do
the same, so **the layout must not change**: the object starts with a `MUI_NotifyData` (28 bytes), then the
`MUI_AreaData`. Offsets inside `MUI_AreaData`, checked with static assertions against the headers on
2026-11-18:

| Offset | Field | Public through |
|---|---|---|
| 0 | `mad_RenderInfo` | `muiRenderInfo`, `_win`, `_dri`, `_screen`, `_pens`, `_window`, `_rp` |
| 8 | `mad_Font` | `_font` |
| 12 | `mad_MinMax` (`MinWidth, MinHeight, MaxWidth, MaxHeight, DefWidth, DefHeight`, all `WORD`) | `_minwidth` ... `_defheight` |
| 24 | `mad_Box` (`Left, Top, Width, Height`, `WORD`) | `_left` ... `_bottom` |
| 32 | `mad_addleft`, `mad_addtop`, `mad_subwidth`, `mad_subheight` (`BYTE`) | `_addleft` ... `_subheight` |
| 36 | `mad_Flags` | `_flags` |
| 40 to 76 | weights, IDCMP flags, background spec, font preset, short help (60), fix width/height (64, 68), `mad_VirtualTop` (72), `mad_Flags2` (76) | private, but kept at MUI's offsets because programs read them with MUI's private macros (Voyager's `_vtop` and `_shorthelp`) |

The inner spacing bytes (`mad_InnerLeft` ...) are at 84 in OpenMUI; the source comment says MUI keeps them
at 88. Whether any program reads them is not known (*read*, not tested).

`muiNotifyData(obj)`, `muiAreaData(obj)`, `muiGlobalInfo(obj)`, `muiUserData(obj)` and `muiRenderInfo(obj)`
are the accessors. Every other field and flag marked `PRIV` in `classes/area.h` may change between releases.

## External classes (`.mcc`)

A class in a file is a shared library found by name, exactly as MUI does it.

**Search order.** `MUI_GetClass(name)` (and so every `MUIC_` name `NewObject` does not know) tries the built-in
classes first (case-sensitive compare), then opens, with `OpenLibrary(path, 0)`, in order:

1. `OpenMUI/<name>`
2. `Classes/OpenMUI/<name>`
3. `MUI/<name>`

each relative to `LIBS:` (and resident libraries). So a class in `LIBS:OpenMUI` wins over a same-named class in
`LIBS:MUI`. (*read*, `mui_getclass.c`, `support_classes.c`. The class-split note still says `LIBS:Zune` for the
class-file drawer; that is the earlier name.)

**What the library must provide.** The library's `MCC_Query(0)` (vector 5, `-30`, the first one after the standard four) returns the
`struct MUI_CustomClass *` of the class (an MCP's `MCC_Query(1)` is tried when `MCC_Query(0)` returns NULL);
`mcc_Class` must be set. OpenMUI keeps the library it opened (`mcc_Module`) and closes it when the class is freed. The usual class library does what any MUI `.mcc` does: in its init (run by ramlib) it opens
`muimaster.library` and calls `MUI_CreateCustomClass(libbase, "Area.mui" (or another superclass), NULL, size, dispatcher)`.
The class's name is the library's file name, given by the `base` argument.

**The identity rule.** The init of a class library runs in ramlib's task, not in the program's, so its own `OpenLibrary` of `muimaster.library` would otherwise be decided by the version the class asks for. OpenMUI therefore sets the identity of the program that loads the class
(`IdLoader`) around the load: the class's own `OpenLibrary("muimaster.library", ...)` gets the same
identity as the program's, so a class that branches on the library version (TheBar sizes its virtual bar itself
only below MUI 4; NList and TextEditor test MUI 4 revisions) behaves as the program expects. Consequences,
from the developer guide:

- A class library serves every program of the identity it was loaded for, while any program uses it.
- Two programs loading classes at the same moment can swap the loading identity (no lock).
- The class's own `OpenLibrary` should ask for the version it needs; if it asks for 19 under the
  loading identity MUI 4 or MUI 5 it still gets that identity.

**Why `.mui` names are reserved.** `Name.mui` is the name of a *built-in* class of a `muimaster.library`
(real MUI keeps its own rarely used classes as `MUI:Libs/mui/Pixmap.mui` and so on, and OpenMUI ships its
own as `LIBS:OpenMUI/<Name>.mui`, built from the class sources, which only that library's
build can use). OpenMUI therefore opens a `.mui` file that it has not built in **only when its id string says
it is OpenMUI's**, read from its Resident tag before any of its code runs. A real MUI 4/5 `Pixmap.mui` is
skipped (it would crash OpenMUI). Name your own classes `<Name>.mcc`.

## Checklist

- Call the superclass first in `OM_NEW`, `MUIM_AskMinMax` and `MUIM_Setup`; last in `OM_DISPOSE` and `MUIM_Cleanup`.
- Draw only inside `_mleft`..`_mbottom`, only while `MADF_CANDRAW` is set (call `MUI_Redraw`, never your own `MUIM_Draw`).
- Never keep a pointer to the render environment past `MUIM_Cleanup`.
- Dispose objects before `MUI_DeleteCustomClass`; delete the class before closing `muimaster.library`.
- Do not use `MUIB_ZUNE` extensions or `PRIV` fields in a class that should run on real MUI.
