# Zune's settings program against MUI 3.8's (roadmap phase 7, 2026-11-10)

Branch `prefs38`. Everything about MUI 3.8 here is black-box: its settings
program's pages (screenshots, `~/Projects/Zune68-cleanroom/defaults/shots/`
and new ones in FS-UAE `a1200-31-mui38`), its guide (`MUI:Docs/English/MUI.guide`),
its presets (`MUI:Presets/*.prefs`) and the files it writes when one setting
is changed at a time. No MUI binary was read.

## Inventory: every setting of MUI 3.8's settings program

Columns: the MUI 3.8 setting; its item id in MUI 3.8's file (measured, see
below; "=" means the same id as Zune's); whether Zune prefs offers it;
whether the library honours it ("yes" = the library reads and uses it).

| Page | MUI 3.8 setting | MUI 3.8 id | Zune prefs | Library honours |
|---|---|---|---|---|
| System | Public screen name | 0x24 = | yes | yes |
| System | Call Inspector (button) | - | yes (button) | - |
| System | Pop to front | 0x7c (Zune 0x700) | yes | yes |
| System | Iconification hotkey | 0x29 (Zune 0x701) | yes | yes (since `prefs38`: iconifies and shows again, as measured on MUI 3.8) |
| System | Show: Icon / Menu | 0x14 / 0x7d (Zune 0x702 / 0x703) | yes | yes (since `prefs38`; Menu = a Workbench Tools item) |
| System | On startup (iconified) | 0x15 (Zune 0x704) | yes | yes (since `prefs38`) |
| System | ARexx | 0x7e (Zune 0x705) | yes | yes (since `prefs38`) |
| System | First / next bubble (tenths of a second) | 0x81 / 0x82 (Zune 0x706 / 0x707) | yes | yes |
| Windows | Positions (forget, remember, save on exit) | 0x7a = | yes | yes |
| Windows | Refresh (smart, simple) | 0x1c (Zune 0x404) | yes | yes |
| Windows | Redraw (without, with clear) | 0x1d = | yes | yes (Zune's default stays "without clear": the same pixels drawn once) |
| Windows | Title gadgets Popup, Prefs, Snapshot, Iconify | 0x78, 0x77, 0x76, 0x13, one each (Zune one mask 0x50a) | yes | yes |
| Windows | Fonts normal / tiny / big | 0x1e / 0x20 / 0x23 = | yes | yes |
| Windows | Backgrounds window / requester | 0x37 / 0x38 = | yes | yes |
| Windows | Spacing L R T B | 0x01-0x04 = | yes | yes |
| Windows | Frame thickness (auto, thick, thin) | 0x0e (Zune 0x50b) | yes | yes |
| Groups | Title position / colour / font | 0x0f / 0x10 / 0x22 = | yes | yes |
| Groups | Frame normal / virtual | 0x33 / 0x35 = (types 5-10 renumbered) | yes | yes |
| Groups | Spacing horizontal / vertical | 0x07 / 0x08 = | yes | yes |
| Groups | Backgrounds framed / page / register | 0x5a / 0x95 / 0x54 = | yes | yes |
| Buttons | Text / image button frame | 0x2b / 0x2c = | yes | yes |
| Buttons | Background, pressed background, font | 0x39 / 0x3e / 0x80 = | yes | yes |
| Buttons | Checkmark / radio look | 0x46 / 0x47 = | yes | yes |
| Buttons | Radio spacing H / V | 0x05 / 0x06 = | yes | yes |
| Cycles | Cycle image, popup position / level | 0x48 / 0x12 / 0x11 = | yes | yes |
| Cycles | Popup speed | 0x1a = | yes | yes (since `setleft`: the popup rolled out from the top, below) |
| Cycles | Popup frame / background, recessed entries | 0x34 / 0x3d / 0x19 = | yes | yes |
| Sliders | Container / knob background and frame, knob font | 0x5b / 0x5c / 0x36 / 0x90 / 0x88 = | yes | yes |
| Scrollbars | Arrows, type, knob / background, frame, arrangement | 0x42-0x45 / 0x83 / 0x4c / 0x3c / 0x31 / 0x09 = | yes | yes |
| Listviews | Multi, refresh, smooth (and value) | 0x0d / 0x0a / 0x1b, 0x0c = | yes | yes |
| Listviews | Fonts normal / fixed, leading | 0x1f / 0x21 / 0x0b = | yes | yes |
| Listviews | Input / read-only list frame and background | 0x30 / 0x2f / 0x3a / 0x96 = | yes | yes |
| Listviews | Cursor active / selected / both | 0x3f / 0x40 / 0x41 = | yes | yes |
| Strings | Frame, popup images default / file / drawer | 0x2e / 0x49-0x4b = | yes | yes |
| Strings | Inactive / active background and text | 0x84-0x87 = | yes | yes |
| Navigation | Drag: left button + qualifier | 0x89 / 0x8b = | yes | yes (since `prefs38`) |
| Navigation | Drag: middle button + qualifier | 0x8a / 0x8c = | yes | **no** |
| Navigation | Drag: autostart + distance | 0x8d / 0x8e = | yes | yes (since `prefs38`; was a fixed 3) |
| Navigation | Drag frame / look | 0x18 / 0x94 = | yes | frame yes, look **no** |
| Navigation | Balancing groups look | 0x7b = | yes | yes |
| Navigation | Colour of active object | 0x8f = | yes | yes (since `setleft`, pixel-identical with MUI 3.8) |
| Navigation | The 24 keys | 0x5f-0x74 = | yes | yes |
| Special | Text field frame / background, gauge frame | 0x2d / 0x3b / 0x32 = | yes | yes |
| Special | Device images, tape images | 0x4d-0x53, 0x55-0x59, 0x5d, 0x5e = | yes | yes |
| MagicWB | (information only) | - | - | - |
| Listtree | Listtree.mcc's page (a third-party class shipped with MUI 3.8) | 0x8002xxxx (the class's own) | **no** | - (no Listtree in Zune) |
| Busy | Busy.mcc's page (third-party, shipped with MUI 3.8) | the class's own | **no** | - |

Zune prefs has more than MUI 3.8: string cursor and marked colours
(0x400-0x402), register "truncate titles" (0x403), screen mode (0x505-0x508),
active object look, custom frames (0x600-), the Frames page, "Behave as"
(MUI 3.8/4/5 per program) and the settings pages of the MCCs installed.

So: **Zune prefs already offers every MUI 3.8 setting** except the two
third-party class pages. The library ignored the ARexx switch, the drag
settings and the iconification settings (hotkey, icon, menu, iconified at
start); since `prefs38` it honours them. It still ignores the cycle popup
speed, the middle-button drag, the drag look and the active object colour.

## MUI 3.8's settings files

Measured on FS-UAE `a1200-31-mui38` (OS 3.1, MUI 3.8 unregistered),
2026-11-10: MUI's settings program, one setting changed at a time, Save
(or Use in "Test mode"), the file compared.

- Files: `ENVARC:MUI/«Global».prefs` (global; the name has ISO-8859-1
  guillemets) and `ENVARC:MUI/<BASE>.<n>.prefs` per program (e.g.
  `MUI.prefs` for the settings program itself), copied to `ENV:MUI` on Use.
- IFF `FORM PREF`: `PRHD` (6 bytes; the first byte 0x13 in files 3.8 writes,
  0x0c in its presets), then `MUIC`: items of a 4-byte id, a 4-byte length and
  that many bytes, **no padding between items** (odd lengths follow each
  other). This is Zune's own format: Zune took its ids from MUI's files.
- **Only settings that differ from the defaults are written**: a Save with
  nothing changed gives a 34-byte file (PRHD and an empty MUIC).
- Numbers are 4-byte big-endian; frames are 7-byte strings ("202211\0");
  images and backgrounds strings ("0:139", "2:m2", "3:XEN/Plain/11pt/Cycle.mbr");
  fonts "name/size"; qualifier and key strings ("alt", "-repeat tab").
- Window positions are not MUIC items: a `MUIW` chunk holds, per window,
  its 4-byte id, length 40, then WORDs: x, y, width, height, a second box (the
  same here), and more words MUI 3.8 keeps (zero, one 1).
- Other classes keep their settings in the same MUIC chunk under their own
  ids: the presets carry Listtree.mcc's under 0x8002xxxx (64-byte strings).
- Ids that differ from Zune's (all found by changing that one setting):
  title gadgets Iconify 0x13, Snapshot 0x76, Prefs 0x77, Popup 0x78 (1 = on;
  known since 2026-11-05); pop to front 0x7c, hotkey 0x29 (string), show icon
  0x14, show menu 0x7d, on startup 0x15, ARexx 0x7e, first bubble 0x81 and
  next bubble 0x82 (tenths of a second: 22 = 2.2 s), frame thickness 0x0e (0
  auto, 1 thick, 2 thin), refresh 0x1c (0 smart, 1 simple).
- Values Zune reads the same way, checked: positions 0x7a (2 = save on exit),
  redraw 0x1d (0 = without clear), group title position 0x0f (1 = centered)
  and colour 0x10 (2 = 3D), drag qualifiers 0x8b / 0x8c (strings), middle
  button 0x8a.
- Frame types 5-10 are numbered differently from Zune's (pixel-compared
  2026-11-05, `~/Projects/Knowledge/amiga/mui-38-frame-thickness.md`):
  MUI 3.8's 5, 6, 7, 8, 9, 10 are Zune's 6, 9, 10, 8, 7, 5.
- `3:<name>` in MUI 3.8 is any image from MUI:Images, brushes (`.mf0`,
  `.mbr`) as well as BOOPSI classes (`.image`); Zune's `3:` is a class only,
  its brushes are `4:`.
- Unregistered MUI 3.8 greys out the design settings (frames, images,
  backgrounds, spacing, fonts of groups ...) and cannot Save them in "Test
  mode", only Use them; files from a registered MUI (and the presets) carry
  them, and MUI 3.8 unregistered does not apply them. Zune applies them.
- Ids 0x92 ("control") and 0x93 (empty) appear in the presets but in no file
  3.8 wrote here; they are copied as they are.

## Importing them into Zune

Zune prefs, menu Project / "Import MUI 3.8 settings...", and a question when
it starts with no Zune settings (`ENV:zune/global.prefs` and
`ENVARC:zune/global.prefs` missing) and `ENVARC:MUI` has MUI 3.8's global
file. The MUI files are only read.

- The global file is converted, shown in every page and tested (as Open
  does): Save or Use keeps it, Cancel goes back.
- Each program's own file is converted into `ENVARC:zune/<same name>` and
  `ENV:zune/<same name>`, unless Zune already has one of that name (Zune's
  per-program files are named the same way, `<BASE>.1.prefs`).
- The converter (`workbench/prefs/Zune/mui38prefs.c`, plain C, tested on the
  PC by `tests/check_mui38_import.py` in `make check`) copies every item and
  translates: the ids above to Zune's, the four title gadget items into
  Zune's mask, frame types 5-10, `3:` brushes into `4:`, `MUIW` into Zune's
  window table item (0x509). Pictures and image classes it cannot find (in
  `LIBS:Zune/Images/` or `MUI:Images/`, or the full path) are left out, so a
  preset that names MUI:Images files does not leave Zune drawing nothing.
- Checked in FS-UAE (a1200-31 with Zune): XEN.prefs as the global file and a
  window file: the question, the import, the pages with XEN's values, the
  Iconify and Popup title gadgets on the window, the window file in both
  drawers; a file with ARexx off: after Save, Zune prefs has no ARexx port.
- Iconification, measured on MUI 3.8 first: with "On startup" and "Show:
  Icon" off its settings program started with no window and no icon (shown
  again over its ARexx port); the hotkey "ctrl alt z" hid MUI-Demo and a
  second press showed it again. Zune, FS-UAE: MUI-Demo started iconified
  with a Tools menu item and no icon, the menu item showed it again and went
  away; the hotkey hid and showed it as on MUI 3.8. Zune's commodities port
  was not in the signals MUIM_Application_NewInput returns, so commodities
  messages (Exchange's too) waited for other input: fixed.
- Library cost of the import: none (it is in Zune prefs: 2.9 KB of code there). The settings the library honours now cost it 548 bytes (stripped file 271,256 -> 271,804 bytes).

## Popup speed and active object colour (2026-11-11, branch `setleft`)

Measured black-box on `a1200-31-mui38` with `tests/lean/setleft.c` (a cycle
with six entries, a sortable list, buttons, a string) and
`tests/lean/mpress.c` (mouse input through input.device; `mpress pop N DX
DY` counts video frames from a press until the pixel DX,DY from the pointer
changes).

- **Unregistered MUI 3.8 ignores the Cycles page's "Popup Menu Control"
  (position, level, speed) when it reads a settings file**: they are
  design settings there (greyed; its settings program shows the defaults
  even when the file has others; a file with level 10 still popped up six
  entries). Its "Test mode" with the **Test** button applies them to running
  programs, which is how they were measured.
- **Popup speed is not a delay.** The popup opens on the press at every
  speed (a quick press opens it, nothing cycles; a press on the cycle image
  cycles one step, no popup, no repeat). The speed rolls the popup out from
  the top: rows 14, 30 and 55 below the press (a 58-row popup) showed after
  1, 3 and 7 frames at speed 50, all within 0-2 frames at 10; the whole
  popup took 2, 2-3, 4-5, 5-6 and 7 frames at 10, 20, 30, 40 and 50: about
  speed * height / 400 frames. MUI 3.8 draws its popup with no layer of its
  own (no new window or layer appears).
- Zune: the popup's drawing is a function of its own (`DrawPopup`); with a
  speed above 0 that gives two frames or more, the menu module (Zmenu.mui,
  `zune_popup_rollout`) draws it a band of rows per frame, each band once,
  clipped to it. At 50: rows 14/30/55 after 0/2/6 frames (MUI 3.8 1/3/7);
  at 0 drawn at once as before, and Zmenu.mui is not loaded.
- **Colour of active object** (0x8f, a pen spec, default `m0`): MUI 3.8
  draws the active object's solid frame one pixel outside its box in that
  pen (white = shine by default; `m5` black). It is honoured from the file
  even unregistered. Zune drew the same frame in shine always; now in the
  setting's pen (obtained with the window's MUI pens). Pixel-identical with
  MUI 3.8 at the default and at `m5` (setleft, Tab to the cycle).
- Library cost: popup speed +162 bytes (Cycle +152, the stub 10), active
  colour +132 bytes (Area +56, Window +76); stripped file 267,400 ->
  267,716 bytes.

## Left

- The library still ignores the middle-button drag and the drag look.
  Neither could be measured on MUI 3.8 in FS-UAE: no drag started there
  from synthetic input (`mpress drag`, wasabi's mouse; in setleft's
  sortable list with `MUIA_Listview_DragType` Immediate and in the
  DragnDrop demo, left button with and without the qualifier), as an
  earlier session found on Zune too. MUI 3.8's look choices are Zune's four
  (always solid, ghosted on box (default), ghosted outside box, always
  ghosted). Next: a drag started by hand in `wasabi view`, or input
  events with the real mouse's timing.
- The drag settings' exact MUI 3.8 behaviour (does the qualifier start the
  drag at once?) is not measured; Zune starts it after one pixel with the
  qualifier held.
- Listtree.mcc's and Busy.mcc's pages: their settings are carried over in
  the imported file but Zune has no such classes.
