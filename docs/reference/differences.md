# OpenMUI against MUI 3.8 and MUI 5

Part of the [reference](README.md). Sources: the [developer guide](../developer-guide.md) ("Known
differences from MUI"), the autodoc comments in the sources, the generated class pages and the compatibility
measurements in `docs/lean/README.md`. All results are from emulators (Amiberry, FS-UAE); nothing is
measured on a real Amiga. "MUI 3.8" and
"MUI 5" mean the real products as measured black-box. This page lists differences; the goal is that a
program sees none at default settings.

## What decides the behaviour

OpenMUI behaves as MUI 3.8, MUI 4 or MUI 5 **per program** (README, "Opening the library"). In the sources
this is the test `ZUNE_APP_MUI38(obj)` (the program behaves as MUI 3.8) or `ZUNE_APP_MUI45(obj)` (MUI 4 or 5),
used in Area (the background pattern origin), Rectangle, String, Prop, Notify and Application. So a
difference below marked "MUI 4/5 only" is active only for a program that asked for version 20 or more, or
whose "Behave as" says so.

| Behaviour | MUI 3.8 identity | MUI 4/5 identity |
|---|---|---|
| `MUI_AllocAslRequest` / `MUI_AslRequest` | every requester is asl.library's, with the program's tags, as MUI 3.8 does | a file or font requester whose tags the MUI panel supports opens as a MUI panel (Filepanel, Fontpanel) in a separate process |
| Background pattern origin (Area) | 0 | the object's left/top (`_left`, `_top`) |
| String editing keys | edited directly, as MUI 3.8 | the Textinput notification protocol and key release offered to every handler (MUI 4/5 behaviour; the methods are in the `Zmui45.mui` module) |
| A window larger than the screen | MUI 3.8's way (moved up, or reopened smaller, or closed) | MUI 5's scrollable contents, only for programs that get MUI 5 |
| Library version the program reads | 19.35 | 20.6000 (MUI 4) or 21.227 (MUI 5) |

## Not implemented

*From the generated class pages; "not handled" means no built source mentions the attribute.*

| Name | Status |
|---|---|
| `muiscreen.library` | headers and `.sfd` in the SDK; no implementation in the tree |
| `MUIC_Scrmodelist` | declared in `classes/list.h`; no class of that name is built |
| `MUIA_Window_FancyDrawing`, `MUIA_Window_Menu`, `MUIA_Window_MouseObject`, `MUIA_Window_NeedsMouseObject`, `MUIA_Window_UseLeftBorderScroller` | declared in `window.h`, not implemented by Window (its autodoc says so) |
| `MUIM_Application_Iconify` | declared, not handled by the Application dispatcher |
| `MUIA_DoubleBuffer`, `MUIA_ExportID` | declared in `area.h`, not implemented by Area |
| `MUIA_Prop_DoSmooth`, `MUIA_Prop_Slider` | declared, no consumer in Prop |
| `MUIA_Virtgroup_Input` | declared, no implementation |
| `MUIA_Scrollgroup_FreeHoriz`, `_FreeVert`, `_NoHorizBar`, `_NoVertBar` | declared, no local implementation |
| `MUIA_Popstring_Button` replacement | "button replacement is not implemented" (its autodoc) |
| `MUIA_Penadjust_PSIMode`, `MUIA_Coloradjust_ModeID` | declared, no local implementation |
| `MUIA_List_Prop_*`, `MUIA_List_HorizProp_*` | not handled |
| `MUIA_Textdata_SourceEncoding` | not handled |
| `MUIA_Panel_*` (the collapsible panel's attributes) | not handled: `Panel.mui` in OpenMUI is MUI 4/5's requester-panel base (`MUIM_Panel_Run`); the AROS collapsible Panel, PanelGroup, PanelTitle and DragHandle sources are in the tree but not built |
| RegisterGadget's popup menu, FrameDynamic's hover (MUI 4/5) | not done; `MUIA_FrameDynamic` is kept, not acted on |
| Busy's MUI 5 look (grey with blue blocks) | not done |
| Popscreen's `WindowHook` | not set (MUI 3.8 sets it) |
| The custom frames (settings `FST_CUSTOM1` to `16`, pictures loaded per window) | drawn (added in 1.0, see the changelog) |

## Compatibility (measured against MUI 3.8)

*Measured in compatibility round 5 (2026-11-18) and in the round of 2026-11-23.*

- **The group spare-pixel rule is a fit, not MUI 3.8's whole rule**: it reproduces 1,278 of 1,346 chosen
  layouts and 1,152 of 1,399 random ones (since 2026-11-23, when the first child's given-up pixel went in:
  with fewer pixels left than children that can grow, each gets one and the excess is taken from the first
  child on; 1,220 and 1,133 before). Still different: with a child other than the first at its limit MUI
  3.8 gives the odd pixel to the first free child (58/20/57 of 135; OpenMUI 57/20/58), and a first child at
  its minimum takes the odd pixel (101/42/42 of 185; OpenMUI 100/42/43). In round 5 six suite windows had
  moved a little away from MUI 3.8 (YAM 2.5's message list one pixel shorter, NFS-GUI, IBrowse 2.5,
  MultiRen, RO, TheBar demo 9) and one demopix window (Subtask), while 13 came nearer.
- **Strings and the right button**: over a String OpenMUI sets WFLG_RMBTRAP (its own edit menu) in a MUI 3
  program too; MUI 3.8 does not (`idcmpprobe hold`; as before this round).
- **Window sizes**: NFS-GUI's list is 1 pixel off; TinyMeterPrefs' default height; the HTMLtext demo is 1
  pixel narrower, and its string is not active at the start (MUI 3.8's is). Pointers is 640x244 against
  640x256. What makes MUI 5 wrap a too-large window (a plain probe window opened clipped, without
  scrollbars) is not known.
- **Frame specifications**: MUI 3.8 does not parse frame spec strings in `MUIA_Frame`; OpenMUI does for every
  program (MUI 4/5 behaviour).
- **Cycle chain**: an object that joins the chain while its window is open goes to the chain's end (MUI 3.8 not
  probed for that alone).
- **Groups with a zero-weight Rectangle beside a Space** (`HGroup(VGroup(object, VSpace(0)), HSpace(0))`)
  get a box reaching across the screen and a test hung reading it; not compared with MUI 3.8, not traced.
- **Programs that fail on OpenMUI for their own reasons**: Voyager 3.5.3 can reset the machine after a quit by
  its close gadget (its own blit is still running when its own `FreeBitMap` frees the bitmap; reproducible
  only on OpenMUI; decided as Voyager's bug). NewsCoaster 1.61 resets at its first start or at quit
  (a stray write of its own lands in the library's code). AmigaGPT keeps
  MUI 3.8's geometry because it asks for version 19. AnimMaker's resets are its own (6.2 % of boots on
  OpenMUI and 6.4 % on MUI 3.8).

## Size and speed

Emulator figures (FS-UAE, `opbench`, `memtally`); real-hardware speed has not been measured.

- The library file is 234,760 bytes (stripped); class files in `LIBS:OpenMUI` carry the rarely used
  classes. The first MUI-Demo program uses 427,288 bytes against MUI 3.8's 460,296, each further
  program 188.6 KB against 210.6-218.9 KB (2026-11-23).
- Instance data per object (`tests/native/instsizes.c`): Area 108 bytes as MUI 3.8's; Text 34 against
  16, Image 20 against 16, Rectangle 4 against 6, Group 36 against 42, Prop 116 against 56, List 324
  against 390, Window 480 against 590.
- A settings file is read by the first program and shared (`ENV:OpenMUI/global.prefs`, checked by date,
  size and key at each program's start); MUI 3.8 was not measured for that.
- Creating an application with its windows takes 0.81-0.82 of MUI 3.8's time (A1200 and A600 emulation, 2026-11-19).
  MUI 3.8 pools its objects; a pool in OpenMUI cost 0.8 to 23 KB per program and was removed.

## Identity and the library

- A class library serves every program of the identity it was loaded for, while any program uses it.
- Two programs loading classes at the same moment can swap the loading identity (no lock); a class whose
  init loads another class through ramlib would deadlock on one.
- `SetFunction()` on `muimaster.library` patches the base in the library list; programs holding an identity
  copy call the original functions.
- More than 32 programs with the library open: the others get no identity record (the rules still apply).
- Open takes the requested version from `D2` (exec 40.10 and 47.8); another exec that passes neither `D0` nor
  `D2` would make every program MUI 3.8 under Automatic.
- `MUIMASTER_VMIN`/`MUIMASTER_VLATEST` in the generated header are 0 (MUI's own header: 11 and 19).
- Settings go to `ENV:OpenMUI` / `ENVARC:OpenMUI`, never into a real MUI's `ENV:MUI`; a program's own
  `MUIM_Application_Save` writes `ENV(ARC):MUI/<base>.cfg`, as MUI does.

## Where MUI 3.8 and OpenMUI agree, measured

For orientation: the MUI 3.8 demos (19 of them) and MUI-Demo's sub-windows open pixel for pixel as MUI 3.8
(`demopix.sh`, round 1); 35 of 36 old and 36 of 48 new real MUI 3 programs in the suite run and look as on MUI
3.8, plus 14 of 16 newer ones (`docs/production.md` gate 1, 2026-11-12). `MUI_CreateCustomClass`'s
`supername` precedence, a Window's `_window()` pointer being NULL, the cycle chain's treatment of hidden and
disabled objects and `MUIA_Application_RexxHook`'s port naming were each measured against MUI 3.8 (autodocs,
`tests/lean/`).
