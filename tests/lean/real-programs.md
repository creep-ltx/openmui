# The real-program suite

Real MUI programs, started on Zune and on real MUI, compared side by side. Every change of the
lean edition is checked with it (roadmap phase 1). The programs are freely downloadable; they are
fetched into `~/.cache/zune68k-real/` (never committed, never redistributed) and run only in the
FS-UAE test machines.

## Running it

```
tests/lean/scripts/suite-fetch.sh                 # download and unpack every archive (once)
make -C workbench/libs/muimaster -f GNUmakefile core prefs plugins BUILD=/tmp/suite/build LDFLAGS=-s
tests/lean/scripts/suite.sh zune  --lib /tmp/suite/build/muimaster.library   # port 1410
tests/lean/scripts/suite.sh mui38                                            # port 1411
tests/lean/scripts/suite.sh mui5                                             # port 1412
tests/lean/scripts/suite.sh zune --groups mui5 --tag zune5                   # MUI 4/5 programs on Zune
tests/lean/scripts/suite-compare.py $ZWORK/suite-real/zune $ZWORK/suite-real/mui38
tests/lean/scripts/suite-compare.py $ZWORK/suite-real/zune5 $ZWORK/suite-real/mui5
```

PATH needs the toolchain (`~/opt/amiga-gcc16/bin`, the suite adds it when it is there) for
`menupos`. A whole run of the 36 MUI 3.x entries takes about 70 minutes per target (every
program on a freshly booted machine); `suite.sh zune --update ID...` runs some again and keeps
the other lines. The contact sheets: `$ZWORK/suite-real/compare-zune-mui38.html` and
`compare-zune5-mui5.html`.

- `suite.sh TARGET [ID...]` boots its own instance (a fresh copy of the machine), copies the
  library (Zune), the classes, the libraries and the programs into the instance's drive folder on
  the PC before it boots, then for each program: start, wait for its first window, answer the
  start-up requesters, screenshot, open the listed extra windows (menus) with a screenshot each,
  quit (its Project/Quit menu item, then the close gadgets, "really quit?" requesters answered,
  Ctrl-C last), and a second run between two `avail` (no flush, see the traps) to measure the
  memory it keeps. Two targets can run at once (ports 1410-1414 belong to this suite).
  `--no-leak` skips the second run, `--keep` leaves the instance running.
- Results: `$ZWORK/suite-real/TAG/results.tsv` (`$ZWORK` defaults to `~/.cache/zune68-lean`), one
  line per program: id, status (`ok`, `nowindow`, `exited`, `reset`: the machine rebooted by
  itself, `frozen`: wasabid stopped answering, `,alert`), still running after quit (1, or 2 when
  the second run did not quit), bytes lost on the second run (negative: more free afterwards), the
  windows as `title@x,y wxh` (screen pixels) and the extra windows. Screenshots `TAG/<id>.png`,
  `TAG/<id>-extraN.png`, log `TAG/log.txt`.
- `suite-compare.py A B` writes `compare-A-B.html` next to them: per program both screenshots
  side by side, the windows and their size differences, status and memory, and prints every
  program that behaves differently.
- The program table, the classes and the start-up hooks are in `tests/lean/real/programs.sh`;
  the machinery in `scripts/suite-lib.sh`. The SYS: volume's name is asked from the instance at
  run time (`assign`), nothing names a volume.
- Menus are picked with the right mouse button at the place `C:menupos` (`tests/lean/menupos.c`,
  built by the suite) reads from the window's menu strip: `wasabi ui menu` sends a message that
  MUI windows (3.8 and Zune alike) ignore. MUI buttons are pressed by their underlined letter or
  by a click relative to the window: `wasabi ui list` sees MUI 3.8's objects but none in Zune's
  windows, so labels would only work on one side.

## Targets

| target | machine | MUI | classes go to |
|---|---|---|---|
| zune | a1200-31 (OS 3.1, 68EC020 + 68882, 2 MB chip + 8 MB fast) | the library under test, its class files in `LIBS:Zune` | `LIBS:MUI` (Zune's own builds from `make plugins` in place of MUI 3.8's Busy and Listtree, and of the classes MUI 4/5 bring (Aboutbox, Calltips, Clock...), as Zune's installer puts them) |
| mui38 | a1200-31-mui38 (same + MUI 3.8) | MUI 3.8 | `MUI:Libs/mui` (`LIBS:mui` does not exist there; MUI 3.8's own Busy, Listtree, Crawling kept) |
| mui5 | a1200-32-mui5 (OS 3.2.1 + MUI 5, 8 MB fast) | MUI 5.0-20210831 | `MUI:Libs/mui` (MUI 5's own classes kept) |

All three get the same third-party class binaries (below); only a class the MUI install already
has is left alone on MUI 3.8 and MUI 5.

## The programs

Chosen by what people use on a MUI Amiga: mail, news, web, IRC, telnet, FTP, a database, system
and file tools, an editor, a player's settings, plus the MCC demos the suite had before. All are
free to download; the licence column says on what terms (for this suite: private testing only,
never redistributed, never committed). `ID` is the name in `programs.sh`.

### MUI 3.x era (group mui3: must run on MUI 3.8)

| ID | program | version | where from | licence | what it exercises |
|---|---|---|---|---|---|
| nlist-demo, nlisttree-demo | NList demos | MCC_NList 0.128 | Aminet dev/mui/MCC_NList-0.128.lha | LGPL | NList, NListview, NListtree, NFloattext, NBalance; two windows |
| texteditor-demo | TextEditor demo | MCC_TextEditor 15.56 | Aminet dev/mui/MCC_TextEditor-15.56.lha | LGPL | TextEditor, its own process |
| thebar-demo1..10 | TheBar demos | MCC_TheBar 26.22 | Aminet dev/mui/MCC_TheBar-26.22.lha | LGPL | TheBar/TheButton, MUIA_Group_Horiz after creation, KnowsDisabled |
| textinput-demo | Textinput demo | MCC_TextInput 29.5 | Aminet dev/mui/MCC_TextInput.lha | freeware (Vapor) | Textinput, Textinputscroll |
| yam25 | YAM (mail) | 2.5 | github.com/jens-maus/yam releases | GPL | NList, TextEditor, BetterString, TheBar, codesets; config and address book windows |
| muimine | MUIMine (game) | 1.2 | Aminet game/think/MUIMine.lha | freeware | custom class drawing, images |
| installerng | InstallerNG (installer) | 1.5 pre | Aminet util/sys/InstallerNG.lha | freeware | Image buttons, Radio, pages (its own Install script) |
| amftp | AmFTP (FTP) | 1.91 | Aminet comm/tcp/AmFTP191.lha | freeware (Vapor) | Busy, Listtree, Textinput; its prefs program first |
| atc | AmiTradeCenter (FTP) | 1.4 | Aminet comm/tcp/AmiTradeCenter.lha | freeware | small toolbar window |
| simplemail | SimpleMail (mail) | 0.45 | sourceforge.net/projects/simplemail | GPL | NList(tree), TextEditor, BetterString, TheBar, Popplaceholder, expat |
| newscoaster | NewsCoaster (news) | 1.61 | Aminet comm/news/newscoaster.lha | open source | Toolbar 15.6, NListtree, NFloattext, TextEditor; first-run user window |
| ibrowse25 | IBrowse (web) | 2.5.9 (68020) | ibrowse-dev.net download page | commercial, demo mode without a key (30 min) | NList family, Newstring, StringHistory, TextHistory, IBCacheBrowser, big windows |
| amirc | AmIRC (IRC) | 3.6.2 (68000) | Aminet comm/irc/amirc_362_68000.lha | freeware since 3.6 ("no key required") | Listtree, NListtree, Textinput, Popplaceholder, Busy, CManager.library |
| wookiechat | WookieChat (IRC) | 2.11 | Aminet comm/irc/WookieChat2.11_OS3.lha | freeware | NList, NListtree, BetterString, NBalance, NBitmap; server window |
| amtelnet | AmTelnet II (telnet/SSH) | 2.4 | Aminet comm/tcp/amtelnet.lha | shareware, 15-minute demo | Term.mcc, Textinput, Popplaceholder; demo requester |
| scout | Scout (system monitor) | 3.6 | Aminet util/moni/Scout_os3.lha | open source | NList, NListtree, Urltext |
| muibase | MUIbase (database) | 4.5 | sourceforge.net/projects/beebase | GPL | NList, BetterString, TextEditor, a 1.3 MB program |
| homebank | HomeBank (accounts) | 3.1 | Aminet biz/misc/HomeBank.lha | freeware | NList, NListtree, BetterString |
| cmanager | Contact Manager | 34.2 | Aminet comm/misc/CManager.lha | shareware without limits | TheBar, Urltext, Lamp, Calendar, Textinput, Popplaceholder; an ASL drawer requester on first start |
| ro | RO (file manager) | 1.27 | Aminet util/dir/RO_V127.lha | shareware ("crippled": reminders) | Register pages, lists, a "really quit?" MUI requester |
| multiren | MultiRen (renamer) | 1.71 | Aminet util/misc/MultiRen.lha | freeware | NList, TextEditor, BetterString, Busy; two windows, preferences |
| archiver | Archiver (archive GUI) | 2.3.24 | Aminet util/arc/Archiver.lha | freeware | big vertical window, a problem requester (no C:Lha) |
| annotate | Annotate (editor) | 3.1.1 | Aminet text/edit/Annotate_usr.lha | GPL | TextEditor, NListtree, a 620 KB program from 2025 |
| amigaampprefs | AmigaAMP-Prefs (player settings) | 1.25a | Aminet mus/play/AmigaAMP-Prefs.lha | freeware | small window, pages |
| viewcsv | ViewCSV (CSV viewer) | 1.3 | Aminet biz/dbase/ViewCSV.lha | freeware | NList with columns, BetterString |
| todo | ToDo (task list) | 1.0 (2026) | Aminet util/misc/ToDo.lha | freeware | Register, List, String: plain MUI 3.8 classes, written in 2026 |

### Programs that need or want MUI 4/5 (group mui5)

Programs that run *only* on MUI 4/5 on a 68k Amiga are rare: of the candidates read, only
DizzyTorrent asks for MUI 3.9+; the others need 3.8 and use MUI 4/5 features where present. They
are run on MUI 5 (and on Zune) for the MUI 4/5 phase.

| ID | program | version | where from | licence | needs |
|---|---|---|---|---|---|
| yam210 | YAM (mail) | 2.10dev (2026-07-26 nightly) | github.com/jens-maus/yam nightly builds | GPL | MUI 3.8+, uses MUI 4 features when there |
| ibrowse30 | IBrowse (web) | 3.0a | ibrowse-dev.net | commercial, demo mode without a key | MUI 3.8, recommends 3.9+ |
| dizzytorrent | DizzyTorrent (BitTorrent) | 2.22 | Aminet comm/tcp/dizzytorrent2.lha | BSD-style | MUI 3.9+, 68020, 8 MB fast |
| amigagpt | AmigaGPT (AI chat) | 3.4.0 | Aminet util/misc/AmigaGPT.lha | MIT | MUI 3, "MUI 5 recommended for all features"; Aboutbox, Guigfx, ActionTextEditor, AudioPlayer |
| mqttexplorer | MQTTExplorer | 0.3 | Aminet comm/net/MQTTExplorer.lha | freeware | MUI 3.8+/4.x, Listtree |

Looked at and left out: Executive 2.10 (its program is packed inside its installer), Daywatch 1.2
(the archive unpacks with broken names), MysticView (not a MUI program), AmigaAMP (skinned,
not MUI; its MUI preferences are in), Voyager (no longer downloadable), AnArchiver (not MUI).

### Classes and libraries (same files on every target)

| class / library | from | licence |
|---|---|---|
| NList, NListview, NListtree, NFloattext, NBalance, NBitmap (+ mcp) | MCC_NList 0.128 | LGPL |
| TextEditor (+ mcp) | MCC_TextEditor 15.56 | LGPL |
| BetterString, HotkeyString (+ mcp) | MCC_BetterString 11.36 | LGPL |
| TheBar, TheBarVirt, TheButton (+ mcp) | MCC_TheBar 26.22 | LGPL |
| Toolbar 15.6, InfoText | NewsCoaster's archive | freeware (Benny Kjær Nielsen) |
| Textinput, Textinputscroll 29.5, vapor_toolkit.library | MCC_TextInput (Aminet) | freeware (Vapor) |
| Popplaceholder 15.5 | MCC_Popph (Aminet) | freeware |
| Urltext 19.7 | MCC_Urltext (Aminet) | freeware |
| Busy, Crawling, Listtree, ListtreeName | MUI 3.8 user archive (Aminet util/libs/mui38usr.lha); on Zune: Zune's own Busy and Listtree (`make plugins`) | MUI 3.8: shareware |
| Term | AmTelnet's archive | Vapor |
| Calendar, Lamp, CManager, MiniMailer, CManager.library | Contact Manager's archive | shareware without limits |
| Newstring, StringHistory, TextHistory, IBCacheBrowser | IBrowse 2.5.9's archive | IBrowse's |
| Guigfx, guigfx.library, render.library | MCC_Guigfx (Aminet) | freeware |
| ActionTextEditor, AudioPlayer | AmigaGPT's archive | MIT |
| codesets.library 6.22 | Aminet util/libs/codesets-6.22.lha | LGPL |
| expat.library | SimpleMail's archive | MIT |

## Results, 2026-11-06 (lean branch at f3761b9e)

Library built as above (68000 build, GCC 16.2), FS-UAE. *Verified*: every line comes from
`suite.sh` runs (programs re-run with `--update` after their start-up hooks were fixed); the
AmIRC and HomeBank resets, Archiver and MultiRen's preferences were also reproduced by hand. Emulated, not real hardware.

**MUI 3.x programs, Zune against MUI 3.8 (36 entries):** 11 behave the same (same windows,
same sizes, both quit, nothing kept). 6 fail on Zune and not on MUI 3.8: three reset the machine
(AmIRC and HomeBank at start, MUIMine when it quits), Archiver loops without a window, IBrowse
2.5.9 opens no main window, and MultiRen's preferences window does not open. The other 19 run,
with windows of other sizes (most within a few pixels; a few use topaz where MUI 3.8 fell back
to its small font), disabled buttons not ghosted, an extra list scroller, or Zune's own file
requester. No program keeps memory on Zune: the second run left at most 48 bytes (MUI 3.8: up to
63 KB for YAM 2.5; negative figures are memory the first run had and the second gave back).

**Update, branch `crashfix` (2026-11-06):** AmIRC, HomeBank, MUIMine and Archiver now run and
quit as on MUI 3.8 (rows below marked "crashfix"; causes and fixes in docs/lean/README.md,
"Crashes found by the real-program suite"). The whole MUI 3.x suite run again on that branch:
32 of 36 lines as in the run above, the 4 that differ are these four (NList-Demo, the first
program after boot, was once "frozen" before its window and twice ok when run again). IBrowse
2.5.9's missing window is the window fitting; IBrowse's "frozen" second quit is FS-UAE crashing
(8 below).

**Update, branch `suitefix` (2026-11-06):** the differences 1, 4-7, 9 and 10 below worked
through (docs/lean/README.md, "MUI 3.8's behaviour for the suite's differences"). The whole
MUI 3.x suite on that branch: **30 of 36 programs as on MUI 3.8** (14 on `lean` measured the
same day as the baseline); the 16 that now match are AmFTP, AmigaAMP-Prefs, Archiver, MUIMine,
NListtree-Demo, TextEditor-Demo, the Textinput demo, TheBar demos 1, 4, 5, 6, 8 and 9, ToDo,
WookieChat and YAM 2.5 (with its address book); none that matched before differs. Still
different: TheBar demo2 and demo3 (18 px wider), AmIRC (256 against 236 high), AmTelnet (248 at
y 4 against 256 at 0), IBrowse 2.5.9 (no main window) and MultiRen (only MUI 3.8's run "still
running after quit"). Results: `$ZWORK/suite-real/after` (ZWORK ~/.cache/zune68-suitefix).

**Update, branch `identity` (2026-11-06, roadmap phase 8):** each program
gets the MUI it asks for (Automatic; docs/lean/README.md, "Each program gets
the MUI it asked for"); the zune target no longer sets MUI 5 for the mui5
group (`--behave-as MUI5` does). MUI 3.x group: 30 of 36 as on MUI 3.8, all
36 the same as `suitefix`'s run. MUI 4/5 group: YAM 2.10dev, IBrowse 3.0a,
AmigaGPT and MQTTExplorer ask for 19 (MUI 3.8), DizzyTorrent for 20 (MUI 4);
4 of 5 the same as with the MUI 5 setting, AmigaGPT 2 pixels lower.
Results: `/tmp/identity/zwork/suite-real/id3`, `id5` (ZWORK of that run).

**MUI 4/5 programs, Zune against MUI 5 (5 entries):** DizzyTorrent (the one that needs MUI 3.9+)
and AmigaGPT open and quit on Zune as on MUI 5 (sizes differ by 6-72 px); IBrowse 3.0a opens,
but the machine stops answering when its second run quits; YAM 2.10dev gets further on Zune than
on MUI 5 (MUI 5 stayed at "Loading graphics..." for two minutes and the machine reset at quit,
so there is no reference); MQTTExplorer ends at once on both.

Same on both, not a Zune difference: SimpleMail 0.45 (wasabid on FS-UAE's emulated bsdsocket
dies when it quits, the instance is rebooted: no memory figure on either side), MQTTExplorer
(ends at once, also on MUI 5), MUIMine's start (its "Could not open Mine Field Window" error on
both; only the quit differs).

### MUI 3.x programs: Zune and MUI 3.8

Status: `ok`; `reset` (the machine rebooted by itself), `nowindow,busy` (running, never waiting,
no window), `,reset-at-quit`. Still running: 1 = did not end after quit (Project/Quit, close
gadgets, Ctrl-C). Lost: bytes less free after the second run than before it.

| ID | Zune: status, still running, lost | MUI 3.8: status, still running, lost | main windows Zune / MUI 3.8 |
|---|---|---|---|
| nlist-demo | ok, 0, -1976 | ok, 0, -672 | NList-Demo 1996-2014 475x256; NList-Demo 1996-2006 Win 2 325x256 / NList-Demo 1996-2014 475x256; NList-Demo 1996-2006 Win 2 325x256 |
| nlisttree-demo | ok, 0, -144 | ok, 0, 0 | NListtree-Demo 365x256 / NListtree-Demo 367x256 |
| texteditor-demo | ok, 0, 0 | ok, 0, 0 | TextEditor-Demo 438x219 / TextEditor-Demo 504x253 |
| thebar-demo1 | ok, 0, -27456 | ok, 0, -27432 | TheBar Demo1 430x127 / TheBar Demo1 430x124 |
| thebar-demo2 | ok, 0, -11712 | ok, 0, -11664 | TheBar Demo2 366x69; TheBar Demo2 Control 399x256 / TheBar Demo2 388x71; TheBar Demo2 Control 381x256 |
| thebar-demo3 | ok, 0, -26112 | ok, 0, -26112 | TheBar Demo3 399x256 / TheBar Demo3 381x256 |
| thebar-demo4 | ok, 0, -3128 | ok, 0, -3128 | TheBar Demo4 430x125 / TheBar Demo4 430x122 |
| thebar-demo5 | ok, 0, -96 | ok, 0, 0 | TheBar Demo5 430x132 / TheBar Demo5 430x129 |
| thebar-demo6 | ok, 0, -15560 | ok, 0, -15608 | TheBar Demo6 430x125 / TheBar Demo6 430x122 |
| thebar-demo7 | ok, 0, -15608 | ok, 0, -15608 | TheBar Demo7 354x141 / TheBar Demo7 376x143 |
| thebar-demo8 | ok, 0, 24 | ok, 0, -48 | TheBar Demo8 430x125 / TheBar Demo8 430x122 |
| thebar-demo9 | ok, 0, -49832 | ok, 0, -55656 | TheBar Demo9 640x133 / TheBar Demo9 627x110 |
| thebar-demo10 | ok, 0, 48 | ok, 0, 0 | TheBar Demo1 162x64 / TheBar Demo1 156x58 |
| textinput-demo | ok, 0, 0 | ok, 0, -48 | TextInput Demo 454x256 / TextInput Demo 240x256 |
| muimine | ok,reset-at-quit, ?, ?; crashfix: ok, 0, 0 | ok, 0, 0 | MUI Mine 436x131 / MUI Mine 420x131 |
| installerng | ok, 0, -216 | ok, 0, -168 | InstallerNG 1.5 (Feb 21 2001) 300x131 / InstallerNG 1.5 (Feb 21 2001) 300x131 |
| atc | ok, 0, 0 | ok, 0, 0 | AmiTradeCenter 194x65 / AmiTradeCenter 194x65 |
| simplemail | ok,reset-at-quit, ?, ? | ok,reset-at-quit, ?, ? | SimpleMail 0.45 640x256 / SimpleMail 0.45 640x256 |
| newscoaster | ok, 0, -112 | ok, 0, -8168 | NewsCoaster 640x256 / NewsCoaster 640x256 |
| ibrowse25 | ok, 0, ?(frozen at 2nd quit) | ok, 0, -8240 | IBrowse request 276x67; IBrowse Error 574x80 / [1] IBrowse: About IBrowse 2.5.9 480x192 |
| amirc | reset, ?, ?; crashfix: ok, 0, -56 | ok, 0, -72 | crashfix: AMIRC.1 · Server selection 550x256 / AMIRC.1 · Server selection 550x236 |
| amtelnet | ok, 0, -72 | ok, 0, -8192 | amtelnet.1 · not connected (77x23) 640x248 / AMTELNET.1 · not connected (77x24) 640x256 |
| scout | ok, 0, 0 | ok, 0, 0 | SCOUT [OS3/68k] 639x114 / SCOUT [OS3/68k] 639x114 |
| muibase | ok, 0, -128 | ok, 0, -5184 | Unnamed 400x256 / Unnamed 400x256 |
| homebank | reset, ?, ?; crashfix: ok, 0, -4232 | ok, 0, -4176 | crashfix: HomeBank (nobody) (PROGDIR:Wallets/) 640x256 / HomeBank (nobody) (PROGDIR:Wallets/) 640x256 |
| ro | ok, 0, -64 | ok, 0, -5160 | RO 1.27 (5.12.98) -- Copyright © 1994-1998 by Oliver Rummeyer 570x219 / RO 1.27 (5.12.98) -- Copyright © 1994-1998 by Oliver Rummeyer 570x219 |
| archiver | nowindow,busy, 1, ?; crashfix: ok, 0, -56 | ok, 0, -16464 | crashfix: Archiver Problem! 324x139; Archiver V2.3.24 © by Jürgen Späth 27.12.2001 418x249 / Archiver Problem! 184x150; Archiver V2.3.24 © by Jürgen Späth 27.12.2001 321x249 |
| annotate | ok, 0, -1176 | ok, 0, -1128 | Annotate 3.1.1 640x240 / Annotate 3.1.1 640x240 |
| viewcsv | ok, 0, -2552 | ok, 0, -552 | ViewCSV - No file loaded 615x256 / ViewCSV - No file loaded 615x256 |
| todo | ok, 0, 0 | ok, 0, 0 | ToDo 313x167 / ToDo 313x158 |
| yam25 | ok, 0, -336 | ok, 0, 63280 | YAM 2.5  -  User: 640x256 / YAM 2.5  -  User: 640x256 |
| amftp | ok, 0, -216 | ok, 0, -216 | AmFTP.1 · not connected 428x256 / AMFTP.1 · not connected 561x256 |
| wookiechat | ok, 0, -52536 | ok, 0, -65840 | Select server 419x256; Wookiechat 325x256 / Select server 424x256; Wookiechat 325x256 |
| cmanager | ok, 0, -33912 | ok, 0, -50160 | Contact Manager (CONTACTMAN.1) 640x256 / Contact Manager (CONTACTMAN.1) 640x256 |
| multiren | ok, 0, -120 | ok, 1, ? | MultiRen v1.71 by Deniil 715! 484x256 / MultiRen v1.71 by Deniil 715! 484x256 |
| amigaampprefs | ok, 0, 0 | ok, 0, -16384 | AmigaAMP Prefs v1.25 by Deniil 715! 401x252 / AmigaAMP Prefs v1.25 by Deniil 715! 410x252 |
### MUI 4/5 programs: Zune and MUI 5

| ID | Zune: status, still running, lost | MUI 5: status, still running, lost | main windows Zune / MUI 5 |
|---|---|---|---|
| yam210 | ok, 0, -392 | ok,reset-at-quit, ?, ? | Confirmation Request 388x88; Configuration 640x256 / (untitled) 376x201 |
| ibrowse30 | ok, 0, ?(frozen at 2nd quit) | ok, 0, -4568 | [1] IBrowse: About IBrowse 3.0 480x192 / [1] IBrowse: About IBrowse 3.0 480x192 |
| dizzytorrent | ok, 0, 0 | ok, 0, -2696 | DizzyTorrent 2.22(Unregistered) 378x256 / DizzyTorrent 2.22(Unregistered) 372x256 |
| amigagpt | ok, 0, -2736 | ok, 0, -5128 | AmigaGPT Error 580x91; AmigaGPT 590x255 / AmigaGPT Error 508x117; AmigaGPT 576x230 |
| mqttexplorer | exited, 0, ? | exited, 0, ? | - / - |
## Differences on Zune, most programs affected first

Each with how to reproduce: `suite.sh zune ID...` and `suite.sh mui38 ID...` (or `mui5`), then
`suite-compare.py` for the pictures. Phase 6 of the roadmap; what branch `suitefix` fixed is
marked, with its commit in docs/lean/README.md.

1. **Windows wider than the screen are not refitted as MUI 3.8 does** (5 programs). MUI 3.8
   draws such a window in its small font (and tightened spacing), Zune keeps topaz and opens it
   clipped at 640 pixels, or not at all. **Fixed (suitefix)** except IBrowse: MUI 3.8 has five
   stages (helvetica 11; helvetica 9; less spacing; no inner spacing; topaz 8), measured with a
   row of buttons as well as a column; the later windows of an application open in its stage;
   group titles were 1 px too low (the Textinput demo fitted topaz on Zune); 2-D groups with a
   partial last row became one row (MultiRen's preferences, 2168 px wide); Register titles
   widened the group; List widths, a weight-0 child's default width. YAM's main window and
   address book, the Textinput demo, MultiRen's preferences, AmFTP and Archiver now open as on
   MUI 3.8.
   - YAM 2.5 main window (`yam25`): MUI 3.8 small font, every toolbar button and column visible;
     Zune 640 wide in topaz, toolbar cut after "Get", "Subject" cut. Its address book: Zune
     640x256, MUI 3.8 426x256.
   - TheBar demo9 (`thebar-demo9`): MUI 3.8 627x110 with all 13 buttons; Zune 640x133, the last
     four buttons (Search, Filter, Filters, Abook, Config) off the window.
   - Textinput demo (`textinput-demo`): too tall for topaz on MUI 3.8 (it opens 240x256 in the
     small font); Zune 454x256 in topaz.
   - MultiRen, Project/Preferences... (`multiren`, its extra window): MUI 3.8 opens "MultiRen
     Preferences and Global settings" 523x220 with five register pages whose titles it cuts
     ("Open on sta", "More settin"); Zune opens nothing (Project/About... in the same menu does
     open, so the menu works). Probably Register asking for the full width of every title.
   - IBrowse 2.5.9 (`ibrowse25`): Zune "IBrowse request: Couldn't open initial window" plus four
     "IBrowse Error" requesters (the TransferAnimation datatype errors, on MUI 3.8 too); MUI 3.8
     opens its window and the demo notice. Traced (2026-11-06, a trace build): it is the window
     fitting, the main window's minimum height is 367 in topaz, 370, 364 and 361 in the three
     fitting stages, on a 256-line screen. The later "freeze" is the emulator dying, see 8.
2. **The machine resets** (3 programs). **Fixed 2026-11-06** (docs/lean/README.md, "Crashes
   found by the real-program suite"); the suite now gives the same as MUI 3.8 for all three.
   - AmIRC 3.6.2 68000 (`amirc`): reset at start. Cause: AmIRC calls muimaster.library's private
     LVO -264, which MUI 3.8 and MUI 5 have and Zune's jump table (ending at -240) did not: a
     guru, which reboots OS 3.1. Not Listtree's fault (the snoop showed the last call before the
     reset, not the one that crashed).
   - HomeBank 3.1 (`homebank`): reset at start. Cause: Dtpic called datatypes through a NULL
     DataTypesBase (guru 8000000B at address -48); not NList.
   - MUIMine 1.2 (`muimine`): reset when it quit. Cause: a failed setup of its mine field window
     left the root group marked as set up, and its Cleanup reached children never set up, whose
     Cleanup went through NULL into the exception vectors.
3. **Archiver 2.3.24 loops without a window** (`archiver`). **Fixed 2026-11-06**: its menus are
   Menuitems placed directly in the Menustrip, which Zune made items; gadtools' LayoutMenusA()
   never returned on the resulting NewMenu array. Now "Archiver Problem!" and the main window
   open as on MUI 3.8 (their sizes differ: the window fitting, see 1).
4. **Disabled objects are drawn like enabled ones** (3 programs). MUI 3.8 ghosts them with its
   dot pattern; on Zune they look normal. **Fixed (suitefix)**: MUIA_Disabled set bit 31 of a
   WORD, so it never disabled anything (only a disabled group's children were); the ghosting is
   now pixel-identical to MUI 3.8's:
   - MultiRen: Remove, Clear list, Enable, Disable, Select all, Find, Restore, Undo, the string
     gadget, Rename, Undo rename, Set Comments (the list is empty).
   - TextEditor demo: Undo and Redo (nothing to undo yet).
   - AmFTP: the whole main window behind its evaluation notice.
   Compare `multiren.png`, `texteditor-demo.png`, `amftp.png` of the two runs.
5. **Lists get a horizontal scroller that MUI 3.8 does not have** (MUI 3.9's): ToDo (`todo`)
   313x167 against 313x158 (+9 px: the scroller under its list), YAM 2.5's folder list.
   **Fixed (suitefix)**: ToDo's is a Scrollgroup with MUIA_Scrollgroup_FreeHoriz FALSE, which
   Zune ignored (now 313x158); Lists no longer get MUI 4's horizontal scroller when behaving as
   MUI 3.8 (MUI 3.8's List has none). YAM's folder list is NListtree's, not Zune's List; not
   compared again.
6. **MUI_AslRequest() shows Zune's own requester, not asl.library's** (`cmanager`): Contact
   Manager's "Select the directory for CM's database" is an ASL requester on MUI 3.8 (OK,
   Volumes, Parent, Cancel; a system requester) and Zune's own MUI window on Zune (Parent, Back,
   Forward, Save, Cancel), run by another process. Programs and scripts that expect ASL's
   requester (its look, its buttons, its keys) see something else. **Fixed (suitefix)**:
   behaving as MUI 3.8, MUI_AllocAslRequest/MUI_AslRequest/MUI_FreeAslRequest are asl.library's
   calls with the program's tags, as logged on MUI 3.8.
7. **MUI_RequestA() ignores the program's font settings** (`wookiechat`): WookieChat's "Quit
   Program?" is 104x34 in the small font on MUI 3.8 (as all its windows), 188x39 in topaz on Zune.
   **Fixed (suitefix)**: not a font setting but the application's fitting stage, which MUI 3.8
   keeps for the application's later windows; and MUI 3.8's requester layout (measured: five
   requesters with the same objects and sizes now).
8. **IBrowse 3.0a: the machine stops answering when the second run quits** (`ibrowse30`, group
   mui5); its first run opens and quits like on MUI 5. Traced 2026-11-06, not fixed: it is not
   the Amiga that stops but the emulator: FS-UAE 3.2.35 aborts ("malloc(): unaligned tcache
   chunk detected", in its filesystem thread; `coredumpctl`), sometimes already in the first run.
   On the OS 3.1 machine: 4 of 4 tries (two runs each) died on Zune, 0 of 3 on MUI 3.8. The DOS
   packets IBrowse sends to the drive (logged with a PutMsg patch) are the same on both up to
   the crash; what in Zune sets off the corruption is not found. IBrowse 2.5.9's frozen second
   quit is the same.
   Next: run it in Amiberry (8.5, installed; our partner finds it more correct than FS-UAE) to
   tell an FS-UAE fault from a Zune one before chasing it further.
9. **Window sizes** (each a few pixels; the contact sheet shows where). **Mostly fixed
   (suitefix)**: all below match MUI 3.8 now except TheBar demo2's control window and demo3
   (18 px wider), AmTelnet, NListtree's tree images and the MUI 4/5 group (not looked at);
   scrollbar arrows follow the
   font's height, Archiver's vertical sliders (MUIA_Group_Horiz on a Slider) and its Register
   (may grow, thick-frame spacing):
   - TheBar toolbars 3 px taller (demos 1, 4, 5, 6, 8: 125-132 against 122-129), demo2 and demo7
     22 px narrower, demo2's control window and demo3 18 px wider, demo10 +6x+6.
   - TextEditor demo opens 438x219 against 504x253; AmFTP's main window 428 against 561 wide;
     AmTelnet 640x248 at y 4 against 640x256 at 0; AmigaAMP-Prefs 401 against 410 wide;
     WookieChat's server window 419 against 424; NListtree demo 365 against 367; MUIMine's
     error window 436 against 420 wide.
   - MUI 4/5 group against MUI 5: DizzyTorrent 378 against 372 wide; AmigaGPT 590x255 against
     576x230 (its error requester 580x91 against 508x117).
   - NListtree's tree images (WookieChat's server list): triangles on Zune, folder images on
     MUI 3.8.
10. **YAM 2.5's configuration window opens much later on Zune**: with 13 s of waiting it had not
    appeared on Zune and had on MUI 3.8; with 40 s both show it (after ~50 "Error messages" about
    missing pictures). Not timed precisely. **suitefix**: 15 s against MUI 3.8's 12 s (timed by
    the probe: menu picked to window open); before, not within 60 s.
11. Tooling, not seen by programs: `wasabi ui list` shows the objects of MUI 3.8's and MUI 5's
    windows (buttons by label, class Text.mui) and none in Zune's, so scripts that click MUI
    buttons by name work on real MUI only. The suite presses keys or clicks relative positions
    instead.

## Traps found while building it

- `wasabi ui menu` answers "picked" but MUI windows (3.8 and Zune) do nothing: the suite picks
  menus with the right mouse button at the positions `menupos` reads from the menu strip. Not
  every window of a program has the strip (MultiRen's notice has none).
- With SimpleMail running, any `wasabi run`, `put` or `rexx` made wasabid die (FS-UAE's emulated
  bsdsocket); `ping`, `windows`, `grab`, `ps`, `ui` and the mouse kept working. SimpleMail's quit
  ends wasabid too.
- On the MUI 3.8 machine, `avail flush` (even right after booting, before any MUI program) leaves
  the next MUI program without a window, waiting for ever (NList-Demo, then MUI's own Class1
  demo). The suite measures memory without flushing.
- TheBar's Toolbar.mcc (26.2, the Toolbar replacement) left NewsCoaster's main window empty and
  its task waiting on MUI 3.8; the original Toolbar.mcc 15.6 works.
- MUI 3.8's own Listtree.mcc reset the Zune machine (AmIRC); the Zune target gets Zune's own
  Busy and Listtree (and its other own classes for MUI 4/5 programs, Aboutbox etc.) as Zune's
  installer would.
- One program can spoil the next (a task left running, MUI 3.8 after a flush): every program
  starts on a freshly booted instance.
