# Lean edition: classes as files of their own

Step 6 of the lean edition (2026-11-04). Rarely used classes, and the MUI 4/5
classes that MUI 3.8 programs never make, are no longer inside
`muimaster.library`. Each is a small file in `LIBS:Zune/`, loaded the first
time a program makes one of its objects, the way MUI 3.8 ships its rarely
used classes in `MUI:Libs/mui/`.

## The split

**Files (`LIBS:Zune/<Name>.mui`, 35 files, 160 KB on disk):**

- MUI 3.8's own external set: Boopsi, Coloradjust, Colorfield, Dirlist,
  Floattext, Frameadjust, Framedisplay, Gauge, Imageadjust, Imagedisplay,
  Knob, Levelmeter, Numericbutton, Palette, Penadjust, Pendisplay, Popasl,
  Popframe, Popimage, Poppen, Popscreen, Scale, Scrollgroup, Volumelist.
- Balance (inside MUI 3.8; 5 KB of floating point, rarely used).
- MUI 4/5 classes: Fontpanel (with Fontdisplay), Filepanel, Selectorpanel
  (class `Panel.mui`), Keyadjust, Title, Datamap (with Objectmap), Textdata,
  Selectgroup, Process.

**Inside the library:** Notify, Family, Application, Window, Area, Rectangle,
Group, Virtgroup, Image, Configdata, Text, Numeric, Slider, String, Prop,
Scrollbar, Scrollbutton, Register, Menuitem/Menu/Menustrip, Semaphore,
Dataspace, Bitmap, Bodychunk, ChunkyImage, Cycle, Radio, Popstring,
Popobject, Poplist, Listview, List, Mccprefs, Settings, Settingsgroup,
Aboutmui, Crawling, Dtpic. Aboutmui, Settings, Settingsgroup and Virtgroup
are external in MUI 3.8 but a few hundred bytes each here, less than a
file's own overhead (~1.4 KB).

The list is `CLASSFILES` in `workbench/libs/muimaster/GNUmakefile`;
`CLASSFILES=` (empty) builds every class into the library again.

## How it works

- **The file** (`classfile.c`): a library with a Resident tag. Vector 5
  (-30) is MCC_Query and returns NULL; vector 6 (-36) takes the library's
  interface (a0) and a class name (a1) and returns that class's
  `__MUIBuiltinClass` description. Its id string says `Zune68 class`.
- **The library makes the class** from that description through its normal
  built-in path (`ZUNE_MakeBuiltinClass`): `builtinDispatcher`, the class
  cache, reference counting, and the disabled-look drawing of built-in
  classes are the same as for a class inside the library. Which class is
  in which file is generated from the sources (`gcc/classfiles.py header`
  -> `classfiles.h`), so a name that is not in the list never touches the
  disk.
- **Lifetime:** a file stays open as long as the library; it is closed by
  the library's expunge after `ZUNE_FreeBuiltinClasses` has freed its
  classes (`ZUNE_CloseClassFiles`), so no class code is unloaded while a
  class exists.
- **Calls into the library:** the class sources are unchanged. Every
  library function they call goes through a table: `gcc/classfiles.py
  iface` compares the class objects' undefined symbols with the library's,
  writes the table for the library (`cf/classtable.c`) and one 4-instruction
  jump per function for the files (`cf/libclassstubs.a`, one object each,
  so a file links only the jumps it uses). Both sides are stamped with a
  CRC of the function list: a class file only works with the library it was
  built with (the query refuses any other).
- **Library bases:** the file has its own copies (`cfbases.c`), copied from
  the library at the query. Lazily opened libraries (`lazylib.h`) are opened
  through the library and copied again, so the class code's `ZUNE_ASL()`
  etc. work unchanged. libnix/libgcc functions (string, soft float) are
  linked into each file that needs them.
- **Search:** `OpenLibrary("Zune/<Name>.mui")`, i.e. `LIBS:Zune/`, a drawer
  MUI 3.8 does not use. A file that is not Zune68's (id string) is closed
  again.
- **Two libraries at once** (`muimaster.library` and `zunemaster.library`
  both in memory): a class file serves one of them at a time; the second
  then cannot make that class until the first has expunged.

## Measured (MUI-Demo, OS 3.1 + 4 MB fast, FS-UAE)

| | step 5 | step 6 |
|---|---|---|
| muimaster.library file | 431,680 | 322,240 (+ 160 KB in 35 files) |
| library opened only | 432 KB | 330 KB |
| first MUI-Demo | 686 KB | 605 KB |
| each further MUI-Demo | 266 KB | 266 KB |

MUI-Demo loads five files: Floattext, Dirlist, Volumelist, Gauge, Scale.

## Packaging

`tools/release.py` copies `build/muimaster/Zune/*.mui` to `Libs/Zune/` in
the archive, and the installer copies the drawer to `LIBS:Zune/` (one
`copyfiles`, no prompt per file: the files belong to the library).
