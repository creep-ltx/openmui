# The real-program suite

Real MUI programs (84 MUI 3 programs, 24 MUI 4/5 ones, 49 add-on class demos since 2026-11-11), started on Zune and on real MUI, compared side by side. Every change of the
lean edition is checked with it (roadmap phase 1). The programs are freely downloadable; they are
fetched into `~/.cache/zune68k-real/` (never committed, never redistributed) and run only in the
test machines (`~/UAE/TestMachines`), in Amiberry headless since 2026-11-11 (`EMU=fsuae` for FS-UAE).

## Running it

```
tests/lean/scripts/suite-fetch.sh                 # download and unpack every archive (once)
make -C workbench/libs/muimaster -f GNUmakefile core prefs plugins BUILD=/tmp/suite/build LDFLAGS=-s
tests/lean/scripts/suite.sh zune  --lib /tmp/suite/build/muimaster.library   # port 1410
tests/lean/scripts/suite.sh mui38                                            # port 1411
tests/lean/scripts/suite.sh mui5                                             # port 1412
tests/lean/scripts/suite.sh zune --groups mui5 --tag zune5                   # MUI 4/5 programs on Zune
tests/lean/scripts/suite.sh zune --groups mcc --tag zunemcc                   # the add-on class demos
tests/lean/scripts/suite-compare.py $ZWORK/suite-real/zune $ZWORK/suite-real/mui38
tests/lean/scripts/suite-compare.py $ZWORK/suite-real/zune5 $ZWORK/suite-real/mui5
```

PATH needs the toolchain (`~/opt/amiga-gcc16/bin`, the suite adds it when it is there) for
`menupos`. The machines boot in **Amiberry 8.5 headless** by default (`scripts/amiberry-test`,
the same commands and copies as `amiga-test`; `scripts/emu.sh` chooses, `EMU=fsuae` boots FS-UAE
as before). A whole run of the 36 MUI 3.x entries takes about 40 minutes per target on Amiberry
(every program on a freshly booted machine, a boot being 2 s there; about 70 on FS-UAE);
`suite.sh zune --update ID...` runs some again and keeps the other lines. The contact sheets: `$ZWORK/suite-real/compare-zune-mui38.html` and
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
  the second run did not quit), "lost": in-use before the second run minus in-use after it
  (negative: the second run left memory in use; positive: it gave some back), the
  windows as `title@x,y wxh` (screen pixels), the extra windows, and since 2026-11-06 the memory
  in use while the second run's windows are open (bytes, "?" without a second run) and, seventh,
  the same for the first run from a freshly booted machine (everything the program costs: the
  library, its classes, and what MUI keeps after a quit for the next run). Screenshots
  `TAG/<id>.png`, `TAG/<id>-extraN.png`, log `TAG/log.txt`.
- `--tally FILE` (FILE: `tests/lean/memtally` built) runs the second run under `memtally runall`
  and writes its live blocks with the windows open to `TAG/<id>.mt`; `TALLY_RUN=1` the first run
  instead (`<id>-1.mt`); `TALLY_RUN=keep` starts `memtally lib muimaster.library` before the first
  run and dumps with the windows open (`<id>-1.mt`) and after the quit (`<id>-q.mt`). Read them
  with `tools/lean/mtlib.py` (by task, caller, size; `SYMLIB=` a Zune build with symbols names
  Zune's functions) and `tools/lean/mtsizes.py`. MUI 3.8 is a black box there: its callers show
  as muimaster.library.
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
DizzyTorrent and PassPocket ask for MUI 3.9+; the others need 3.8 and use MUI 4/5 features where
present. MUI 5's own demos (15 of them, the ones MUI 3.8 does not have) cover MUI 5's classes and
features directly. All are run on MUI 5 (and on Zune) for the MUI 4/5 phase. Which version each
asks for (snooped, 2026-11-10): the MUI 5 demos and PassPocket 20 (Zune's Automatic: MUI 4),
DizzyTorrent 20, YAM 2.10dev, IBrowse 3.0a, AmigaGPT, MQTTExplorer and TabGroup-Demo 19, LumiZip
11, Ticklish 0 (all MUI 3.8 under Automatic).

| ID | program | version | where from | licence | needs |
|---|---|---|---|---|---|
| yam210 | YAM (mail) | 2.10dev (2026-07-26 nightly) | github.com/jens-maus/yam nightly builds | GPL | MUI 3.8+, uses MUI 4 features when there |
| ibrowse30 | IBrowse (web) | 3.0a | ibrowse-dev.net | commercial, demo mode without a key | MUI 3.8, recommends 3.9+ |
| dizzytorrent | DizzyTorrent (BitTorrent) | 2.22 | Aminet comm/tcp/dizzytorrent2.lha | BSD-style | MUI 3.9+, 68020, 8 MB fast |
| amigagpt | AmigaGPT (AI chat) | 3.4.0 | Aminet util/misc/AmigaGPT.lha | MIT | MUI 3, "MUI 5 recommended for all features"; Aboutbox, Guigfx, ActionTextEditor, AudioPlayer |
| mqttexplorer | MQTTExplorer | 0.3 | Aminet comm/net/MQTTExplorer.lha | freeware | MUI 3.8+/4.x, Listtree |
| mui5-aboutbox ... mui5-title (15) | MUI 5's own demos that MUI 3.8 does not have: Aboutbox, Animals, Calltips, Class4, Clock, ElidedText, EmbeddedImages, GIFAnim, Hyperlink, LayGroup, List1, MenuImages, Pointers, Textdata, Title | MUI 5.0-20210831 | github.com/amiga-mui/muidev releases, MUI-5.0-20210831-os3.lha (`MUI/Demos`; `SDK/MUI` has their sources and the autodocs) | MUI 5's (free download) | MUI 5 classes and features: Aboutbox, Calltips, Clock, Hyperlink, LayGroup, GIFAnim, Process, Pixmap/Rawimage (bzip2, Z), elided text, editable and sortable lists, menu images, pointer types, Textdata, Title (draggable tabs) |
| passpocket | PassPocket (passwords) | 1.1 | Aminet util/misc/PassPocket.lha | freeware, source included | MUI 3.9+ (resets MUI 3.8 at quit), 68020, TextEditor |
| ticklish | Ticklish (to-do list, made with Hollywood) | 1.1.1 | Aminet util/misc/Ticklish_m68k.lha | freeware | MUI 3.8, MUI 5 for button icons; on the AGA test machines it stops at "Error allocating bitmap!" on every target |
| tabgroup-demo | TabGroup demo | MCC_TabGroup 19.2 (2026) | Aminet dev/mui/MCC_TabGroup.lha | open source (in the archive) | TabGroup.mcc: a Register subclass with draggable tabs; MUI 3.8+ |
| lumizip | LumiZip (7-Zip archiver) | 1.0 (2026) | Aminet util/arc/LumiZip.lha | freeware | MUI 3.8+, NList, TheBar, codesets; asks for muimaster v11 |

Added 2026-11-10 (branch `mui45`): found with Aminet's readme search
(`aminet.net/search?readme=MUI+5&arch[]=m68k-amigaos`, also "MUI 4", "MUI 3.9") and MUI 5's
own demos. Looked at and left out then: SonosController, LumiGram, ZodiTV, AmiAuthenticator
(AmiSSL 5 needed, none on the test machines), OWB-m68k 2.1 (68020+FPU, RTG, 256 MB+),
EmojiGear 6.1 (13 MB, OS 3.2 first), AmiNetRadio 4 (skinned player), EvenMore (MUI 3.8 only),
Lupe (not MUI). MUI 5's LayGroup and GIFAnim classes: MUI 5 brings its own (kept on the mui5
target); Zune and MUI 3.8 get the classic Aminet ones (dev/mui/MCC_LayGroup-19.2.lha,
dev/mui/MCC_GIFAnim.lha), as a user of those would install them. TabGroup.mcc goes to every
target.

Looked at and left out: Executive 2.10 (its program is packed inside its installer), Daywatch 1.2
(the archive unpacks with broken names), MysticView (not a MUI program), AmigaAMP (skinned,
not MUI; its MUI preferences are in), Voyager (no longer downloadable), AnArchiver (not MUI).

### Added 2026-11-11 (branch `compat1`): 48 more programs, old and new

Chosen from what people actually use: Aminet's most downloaded MUI programs (its readme search for
"MUI 3.8" and "MUI" sorted by downloads) and the 2026 wave of new MUI programs (the same search
sorted by date). The column "asks" is the muimaster version each one opens (snooped `OpenLibrary`,
2026-11-11): all are MUI 3 programs under Automatic (nothing asks for 20 or more), the 2026 ones
use MUI 4/5 features where present. Left out: programs that need AmiSSL before they show a window
(LumiWeather, LumiGram, LumiReg, AmiCloud, AmiOwnCloud, AmiPlexAmp, AmiSubsonic, twittAmiga, Wet,
ZodiTV), 68040+ (StormDJ, KVICEr), AmiArcadia (OS 3.9+, 24 MB, 11 MB archive), AmiChess (OS 3.5
and 800x600), AAText (RTG), Trinity (its accelerator), AmBoS (needs its installer), Frogger (not
MUI). Group `mui3` (ID as in `programs.sh`):

| ID | program | version | where from | licence | asks | what it exercises / what happens |
|---|---|---|---|---|---|---|
| voyager | Voyager (web) | 3.5.3 (2026-11-03) | Aminet comm/www/Voyager-3.5.3-AmigaOS.lha | freeware since 2026 (no keyfile) | 19, 18, 5 | Textinput, Popplaceholder, SpeedBar, TearOff, CManager; a toolbar with its own images, `[1] Voyager · About Voyager`; **resets the machine when quit by its close gadget on OpenMUI** (below) |
| evenmore | EvenMore (text viewer) | 0.94 | Aminet text/show/EvenMore.lha | freeware | 19, 11 | BetterString, plugins |
| igame | iGame (WHDLoad front-end) | 2.6.1 (2026) | Aminet util/misc/iGame.lha | open source | 19 | NList, lists with columns, a filter string |
| yam29 | YAM (mail) | 2.9p1 | Aminet comm/yam/YAM29p1-AmigaOS3.lha | GPL | 19 | the YAM between 2.5 and 2.10: NList, TextEditor, TheBar, codesets; its configuration window |
| amigg | AmiGG (instant messenger) | 0.43.114 | Aminet comm/misc/amigg.lha | freeware | ? (its task name is cut) | TextEditor, NList; a "graphics resolver" window; it does not end at quit on any target |
| freeciv | Freeciv client | 1.17 | Aminet game/strat/freeciv.lha | GPL | 11 | ends at once on every target (wants its server) |
| apdf | Apdf (PDF viewer) | 1.3 (020, no FPU) | Aminet gfx/show/Apdf_nofpu.lha | freeware | 11 | an ASL "Open..." requester first |
| mcpprefs | MCP's preferences | 1.30 | Aminet util/cdity/MCP130.lha | freeware | 8 | a MUI 2-era prefs program with many pages (reqtools, mcpgfx.library from the archive) |
| screentabprefs | ScreenTab's preferences | 3.2 | Aminet util/cdity/ScreenTab_3_2.lha | shareware | 19, 11 | no window on any target (wants ScreenTab running) |
| tinymeterprefs | TinyMeter's preferences | 4.31 | Aminet util/wb/Tinymeter_V431.lha | freeware | 12 | Register pages, a bitmap logo; 549x237 against MUI 3.8's 553x250 |
| newsrog | NewsRog (news) demo | 1.7b | Aminet comm/news/NewsRogDemo.lha | demo | 19, 11 | its own 30 MCCs in PROGDIR:MUI; a first-start window 272x166 against 280x170 |
| md2 | MicroDot-II (mail/news) | 1.4 | Aminet comm/news/md2_14.lha | Vapor | ? | Textinput, Listtree, CManager; asks for its database drawer (MUI_AslRequest); does not end at quit on OpenMUI and MUI 3.8, ends at once on MUI 5 |
| amplifier | AMPlifier (audio player) | 2.10 | Aminet mus/play/AMPlifier.lha | freeware | ? | a skinned player window with MUI gadgets |
| makecd | MakeCD (CD writing) | 3.2d | Aminet disk/cdrom/MakeCD_3.2d.lha | freeware since 2021 | ? | a big main window, many pages |
| ahiprefs | AHI's MUI preferences | 4.18 | Aminet driver/audio/ahiusr_4.18.lha | LGPL | 19 | `Prefs/AHI_MUI` with ahi.device from the same archive (hook `pre_host_ahiprefs`); reset the MUI 3.8 machine once |
| fiasco | Fiasco (database) | 2.22 | Aminet biz/dbase/Fiasco_main.lha | freeware | ? | gtlayout.library from the archive; does not end at quit on any target |
| amster | Amster (Napster client) | 0.8a | Aminet comm/tcp/Amster-main.lha | open source | 19, 11 | NList, Textinput, Lamp; a configuration window and the main window |
| beebase | BeeBase (database, MUIbase's successor) | 1.2 (2025) | Aminet biz/dbase/BeeBase-1.2.lha | GPL | 19, 17 | the 1.3 MB m68k program; froze MUI 5 at quit once |
| amimodradio | AmiModRadio (module player from the net) | 1.2 (2025) | Aminet mus/play/AmiModRadio.lha | freeware | 19 | its MUI front end `AmiModRadio_MUI` |
| dmodp | D.A.S. ModulePlayer | 3.5b | Aminet mus/play/DMODP35b.lha | freeware | 9 | a MUI 2.2 program: ends at once on every target |
| amfinger | AmFinger | 1.5 | Aminet comm/tcp/AmFinger1.5.lha | freeware | 19, 14, 11 | Term and Busy from its own `mui` drawer |
| abacus | Abacus (board game) | 2.20 | Aminet game/board/abacus220.lha | freeware | 12 | a MUI 3.6 game: images, a "Quit Abacus now?" requester whose buttons have no key letters (the suite presses Return now) |
| address | Address (address book) | 1.0 | Aminet biz/dbase/Address.lha | freeware | 11 | a small list window |
| rqchoicemui | RequestChoiceMUI | 1.1 | Aminet util/sys/2b_RqChoiceMUI.lha | freeware | 11 | MUI_Request from a shell command (`TITLE/A,BODY/A,GADGETS/A`) |
| assampler | Assampler (sound processor) | 0.99 | Aminet mus/edit/Assampler.lha | freeware | 19 | ends at once on every target (wants AHI) |
| cyclone | Cyclone (Fido point) | 1.25c | Aminet comm/fido/Cyclone125c.lha | shareware | 13 | its 68020 build, a first-start window |
| amigamark | AmigaMARK (benchmark) | 1.1 | Aminet util/moni/AmigaMARK-amigaos3.lha | freeware | 19 | ends at once on every target |
| ali | ALI (AmIRC log inspector) | 2.2 | Aminet comm/irc/ALI-amigaos3.lha | freeware | 19 | ends at once on every target |
| lumiamp | LumiAMP (audio player) | 0.5 (2026) | Aminet mus/play/LumiAMP.lha | freeware | 11 | NList; 408x103 |
| lumipass | LumiPass (passwords) | 1.5 (2026) | Aminet util/crypt/LumiPass.lha | freeware | 19 | NList, NListtree, BetterString, TextEditor, TheBar; 556 wide against MUI 3.8's 552 |
| lumitorrent | LumiTorrent (BitTorrent) | 1.0 (2026) | Aminet comm/net/LumiTorrent.lha | freeware | 19 | NList, TheBar; 556 against 552 |
| lumiftp | LumiFTP (FTP/SFTP) | 1.3 (2026) | Aminet comm/tcp/LumiFTP.lha | freeware | 19 | opens its window without AmiSSL; 556 against 552 |
| tunefinder | TuneFinderMUI (net radio) | 1.0 beta (2026) | Aminet comm/misc/TuneFinderMUI.lha | freeware | 19 | a list and strings |
| u64ctl | u64mui (Ultimate64 control) | 0.5.0 (2026) | Aminet comm/misc/u64ctl.lha | freeware | 19 | 500x226 against MUI 3.8's 500x218 |
| amifritzi | AmiFritzi (FRITZ!Box) | 0.5 (2026) | Aminet comm/tcp/AmiFritzi.lha | freeware | 11 | a settings window first |
| nfsgui | NFS-GUI (nfs-handler GUI) | 1.1 (2026) | Aminet comm/net/NFS-GUI.lha | freeware | 11 | **ends with return code 20 on OpenMUI before any window** (below); opens on MUI 3.8 and MUI 5 |
| ami2ha | ami2ha (Home Assistant) | 0.5 (2026) | Aminet comm/tcp/ami2ha-0.5.lha | freeware | ? | its 020 build ends at once on every target (wants a configuration) |
| amihomeassist | AmiHomeassist | 0.9.1 (2026) | Aminet comm/tcp/AmiHomeassist.lha | freeware | 11 | NList; a settings window and the dashboard |
| amiatari2600 | AmiAtari2600's launcher | 0.1 (2026) | Aminet misc/emu/AmiAtari2600.lha | freeware | 19 | a list of games |
| amipg | AmiPG (Roland synth editor) | 17.340 (2026) | Aminet mus/midi/AmiPG_MUI.lha | freeware | 0 | ends at once on OpenMUI and MUI 3.8 (OS 3.1), opens on MUI 5 (OS 3.2.1) |
| xsysinfo | xSysInfo (system information) | 0.12.0 (2026) | Aminet util/moni/xSysInfo.lha | freeware | 15 | `xSysInfo MUI` with identify.library from the archive |
| kompensator | Kompensator (startup editor) | 2.1 (2026) | Aminet util/batch/Kompensator.lha | freeware | 19, 0 | lists and strings |
| rsgui | RSGUI (Roadshow settings) | 1.2 (2026) | Aminet comm/net/RSGUI.lha | freeware | 11 | opens without Roadshow |
| layview | LayView (PCB viewer) | 1.1 (2026) | Aminet misc/sci/LayView.lha | freeware | 19 | an FPU program (68882 on the test machines) |
| 3doc | 3D Object Converter | 1.90 (2026) | Aminet gfx/3d/3doc-m68k-amigaos_v1.90.lha | freeware | ? | a 6 MB program; ends at once on every target |
| aminetuploader | Aminet-Uploader (MUI) | 1.0 (2026) | Aminet util/misc/AminetUploader.lha | freeware | 19, 11 | no window on any target within 40 s |
| jmri | JMRIController (model trains) | 0.92 (2026) | Aminet misc/misc/JMRIController.lha | freeware | 11 | a MUI 3.1 program |
| amidrop | AmiDrop (MUI version) | 1.1 (2026) | Aminet comm/net/AmiDrop_MUI.lha | freeware | 19 | `Setup/AmiDrop_MUI`; MUI 3.8's run did not end at quit once |

### Add-on classes: each class's own demo (group `mcc`, 2026-11-11)

Many programs stand on third-party classes. The group runs each class's demo or test program from
its Aminet archive on all three targets with the same class files (the classes the suite already
installed stay: NList 0.128, TextEditor 15.56, BetterString 11.36, TheBar 26.22, Toolbar 15.6,
Textinput, Popph, Urltext, Busy and Listtree (Zune's own on the zune target), Lamp, Calendar and
MiniMailer from Contact Manager's archive, Guigfx, LayGroup, GIFAnim). "fails everywhere" means
the demo itself needs something the test machines lack (an argument, a file, a library), on MUI
3.8 and MUI 5 as well; those stay in the table as they cost nothing.

| ID | class | version | from | licence | what |
|---|---|---|---|---|---|
| htmlview-demo | HTMLview | 13.4 | dev/mui/MCC_HTMLview-13.4.lha | freeware (beta) | a time-bombed beta: a system requester first (answered by the hook), then the demo |
| toolbar-demo | Toolbar | 15.12's demo on 15.6 | dev/mui/mcc_toolbar-15.12.lha | freeware | buttons from a brush |
| lamp-demo | Lamp | 11.1's demo | dev/mui/MCC_Lamp.lha | freeware | Lamp with Coloradjust and Register pages; 346x227 on OpenMUI, 640x256 on MUI 3.8 (Coloradjust's default size) |
| speedbar-demo1..9 | SpeedBar, SpeedButton, SpeedBarCfg, SpeedBarVirt | 19.4 | dev/mui/MCC_SpeedBar.lha | freeware | nine toolbar demos; demo6 1 px wider on OpenMUI |
| bwin-demo1..3 | BWin | 22.0 | dev/mui/MCC_BWin.lha | freeware | borderless windows with their own drag bar (Boopsi gadgets); **demo1 and demo3 opened no window on OpenMUI, fixed**; demo2 resets every target |
| calendar-demo1..4 | Calendar | 18.3's demos | dev/mui/MCC_Calendar.lha | freeware | demo4 ends at once everywhere |
| monthnavi-demo | MonthNavigator | 16.8 | dev/mui/MCC_MonthNavi.lha | freeware | no window everywhere (date.library?) |
| busy-demo | Busy | 2.4's demo | dev/mui/MCC_Busy2_4.lha | freeware | three Busy bars: thinner and smaller on OpenMUI's own Busy.mcc (136x130 against MUI 3.8's 148x151) |
| hexedit-demo | HexEdit | 14.0 | dev/mui/MCC_HexEdit.lha | freeware | 230 wide against 237 |
| twfmultiled-demo | TWFmultiLED | 12.6 | dev/mui/MCC_TWFmultiLE.lha | freeware | no window everywhere |
| tearoff-demo | TearOffPanel, TearOffBay, CompactWindow | 13.0 | dev/mui/MCC_TearOff.lha | freeware | "Second TearOff Demo" 326 wide against 256 |
| compactwin-demo | CompactWindow | 12.5 | dev/mui/MCC_CompactWin.lha | freeware | ends at once everywhere |
| tablegroup-demo | TableGroup | 1.0 beta | dev/mui/MCC_TableGroup.lha | freeware | 278x142 against 286x151 |
| mailtext-demo | Mailtext | 3.1 | dev/mui/MCC_Mailtext.lha | freeware | same |
| mathstring-demo | MathString | 1.5 | dev/mui/MCC_MathString.lha | freeware | same |
| tipofday-demo | tipboard, tipwindow | 15.8 | dev/mui/MCC_TipOfDay.lha | freeware | the tip window; its title stays "Tip of the day..." on OpenMUI where MUI 3.8 shows the demo's "Custom titles roxx!" (set after MUIM_Tip_Show), and its list has a scroller MUI 3.8 does not show |
| transferanim-demo | TransferAnim | 1.5 | dev/mui/MCC_TransferAn.lha | freeware | no window everywhere (imagepool.library) |
| minimailer-demo | MiniMailer | 20.1 | dev/mui/MCC_MiniMailer.lha | freeware | same |
| htmltext-demo | HTMLtext | 14.8 | dev/mui/MCC_HTMLtext.lha | freeware | an untitled 85x71 window against 143x83 (and 1 = did not end at quit, everywhere) |
| settingswindow-demo | SettingsWindow | 1.01 | dev/mui/MCC_SettingsWi.lha | freeware | no window everywhere |
| mysticview-demo | MysticView | 13.1 | dev/mui/MCC_MysticView.lha | freeware | ends at once everywhere (wants a picture) |
| icon-demo | Icon | 1.1 | dev/mui/MCCIcon_1_1.lha | freeware | same |
| datestring-demo, timestring-demo | DateString, TimeString | 12.3, 12.5 | dev/mui/MCC_DateString.lha, MCC_TimeString.lha | freeware | same (date.library from the archives) |
| envelope-demo | Envelope | 14.1 | dev/mui/MCC_Envelope.lha | freeware | no window everywhere |
| pkb-demo | Pkb (piano keyboard) | 14.6 | dev/mui/MCC_Pkb14_6.lha | freeware | same |
| wheel-demo | Wheel | 1.0 beta | dev/mui/MCC_Wheel.lha | freeware | same (MUI 3.8 froze at its quit once) |
| hotkeystring-test | HotkeyString | 12.1's test on 11.36 | dev/mui/MCC_HotkeyStr.lha | LGPL | no window everywhere |
| popph-demo | Popplaceholder | 15.5 | dev/mui/MCC_Popph.lha | freeware | same |
| urltext-demo | Urltext | 19.7 | dev/mui/MCC_Urltext.lha | freeware | same |
| guigfx-demo | Guigfx | 19.2 | dev/mui/MCC_Guigfx.lha | freeware | ends at once on OpenMUI and MUI 3.8 (OS 3.1: no PNG datatype for its Test.PNG), opens on MUI 5 (OS 3.2.1) |
| laygroup-demo | LayGroup | 19.2 | dev/mui/MCC_LayGroup-19.2.lha | freeware | same |
| gifanim-demo | GIFAnim | 19.5 | dev/mui/MCC_GIFAnim.lha | freeware | 424 wide against 400 |
| nbitmap-demo | NBitmap | 0.128 | dev/mui/MCC_NList-0.128.lha | LGPL | ends at once on OpenMUI and MUI 3.8 (its icon.png: no PNG datatype on OS 3.1); reset MUI 5 once |
| nlist102-demo | NList 0.102's demo on 0.128 | 0.102 | dev/mui/MCC_NList-0.102_68k.lha | LGPL | an old demo on the new class: same |
| tron-demo | Tron (MUI 3.8's example class) | 3.8 | dev/mui/mui38dev.lha | MUI's | ends at once everywhere (its Tron.mcc is not installed) |

Classes and libraries these add to every target (`CLASSES`, `LIBRARIES` in programs.sh): HTMLview
(+mcp), SpeedBar, SpeedBarCfg, SpeedBarVirt, SpeedButton (+mcp), BWin (+mcp), MonthNavigator
(+mcp), HexEdit (+mcp), TWFmultiLED (+mcp), TearOff.mcp, TearOffBay, TearOffPanel, CompactWindow
(+mcp), TableGroup, Mailtext (+mcp), MathString, tipboard, tipwindow, TransferAnim (+mcp),
HTMLtext (+mcp), SettingsWindow, MysticView, Icon, DateString, TimeString, Envelope (+mcp), Pkb
(+mcp), Wheel (+mcp); amitls.library (Voyager), gtlayout.library (Fiasco), identify.library
(xSysInfo), mcpgfx.library and reqtools.library (MCP), date.library, imagepool.library,
mysticview.library. Programs with their own `MUI` drawer (NewsRog, Amster, MicroDot-II, AmFinger,
Assampler) find those classes through the current directory, as on MUI.

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
| TabGroup (+ mcp) 19.2 | MCC_TabGroup (Aminet) | open source |
| LayGroup (+ mcp) 19.2, GIFAnim 19.5 | Aminet dev/mui/MCC_LayGroup-19.2.lha, MCC_GIFAnim.lha (Zune and MUI 3.8; MUI 5 keeps its own) | freeware |
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

**Update, branch `demopix` (2026-11-06, roadmap phase 2):** MUI 3.8's drawing details from the
demo-set comparison (docs/lean/README.md, "The demo set pixel by pixel against MUI 3.8"). The
whole suite twice on that branch: MUI 3.x group 30 of 36 as on MUI 3.8 (the same six differ),
36 of 36 as `identity`'s run in the first, 35 in the second: AmTelnet reset the machine at its
second quit once; three runs of AmTelnet alone right after were clean (left 0 bytes). MUI 4/5
group: all 5 as `identity`'s. Results: `/tmp/demopix/zwork/suite-real/dp3`, `dp3b`, `dp5`, `dp5b`.

**Update, branch `smaller` (2026-11-06, roadmap phase 3):** rarely used code moved into modules
(docs/lean/README.md, "A smaller library"). MUI 3.x group: 30 of 36 as on MUI 3.8 (the same six),
36 of 36 as `demopix`'s run; MUI 4/5 group: 5 of 5 as `demopix`'s. A `wasabi snoop` trace of every
LoadSeg during the run (`tests/lean/scripts/snooplog.sh`) showed the image module of a first try
loaded in ten programs (NListtree's default node images are MUII_TapePlay, TapeDown and Drawer;
with those in the library, CManager, RO and ViewCSV still loaded it), so that module was taken back;
CManager, RO, ViewCSV, the NListtree demo and YAM 2.5 run again on the final build load no new
module. Results: `/tmp/smaller/zwork/suite-real/sm3`, `sm3b`, `sm3c`, `sm5`.

**Update, branch `memory` (2026-11-06, roadmap phase 4):** smaller instance data for every object
(docs/lean/README.md, "Memory per program"). MUI 3.x group: 30 of 36 as on MUI 3.8 (the same
six), 36 of 36 as `smaller`'s run; MUI 4/5 group: 5 of 5 as `smaller`'s. Results:
`/tmp/memory/zwork/suite-real/fin3`, `fin5` (this branch), `base3`, `base5` (`lean` in the same
session), `mui38` (MUI 3.8 in the same session; its in-use column, fetched for comparison).

Since this branch the suite also records each program's **memory in use** (results.tsv's sixth
column): how much more memory is in use while the second run's windows are open than before it
started; libraries and classes are in memory from the first run, so it is the program's own memory
(its code, objects, windows, data). Emulator figures (FS-UAE, OS 3.1, 8 MB fast), bytes:

| ID | lean | memory | change | MUI 3.8 |
|---|---:|---:|---:|---:|
| nlist-demo | 315,416 | 311,728 | -3,688 | 262,056 |
| nlisttree-demo | 931,184 | 928,816 | -2,368 | 909,624 |
| texteditor-demo | 189,960 | 186,456 | -3,504 | 158,008 |
| thebar-demo1 | 210,344 | 208,160 | -2,184 | 194,376 |
| thebar-demo2 | 241,984 | 235,272 | -6,712 | 197,264 |
| thebar-demo3 | 250,320 | 243,456 | -6,864 | 207,104 |
| thebar-demo4 | 190,544 | 188,360 | -2,184 | 174,576 |
| thebar-demo5 | 157,920 | 155,768 | -2,152 | 145,304 |
| thebar-demo6 | 197,160 | 194,976 | -2,184 | 181,192 |
| thebar-demo7 | 189,760 | 189,464 | -296 | 185,600 |
| thebar-demo8 | 183,696 | 181,512 | -2,184 | 167,728 |
| thebar-demo9 | 280,192 | 278,016 | -2,176 | 266,192 |
| thebar-demo10 | 158,904 | 158,368 | -536 | 151,920 |
| textinput-demo | 124,792 | 123,064 | -1,728 | 107,768 |
| yam25 | 1,735,880 | 1,730,000 | -5,880 | ? |
| muimine | 142,824 | 142,432 | -392 | 146,408 |
| installerng | 729,504 | 729,504 | 0 | 729,512 |
| amftp | 417,568 | 405,784 | -11,784 | 308,408 |
| atc | 513,360 | 513,360 | 0 | 513,360 |
| newscoaster | 1,278,912 | 1,259,808 | -19,104 | 1,017,680 |
| amirc | 992,520 | 985,128 | -7,392 | 801,336 |
| wookiechat | 2,349,088 | 2,324,832 | -24,256 | 2,094,160 |
| amtelnet | 370,432 | 366,048 | -4,384 | 300,872 |
| scout | 471,584 | 469,384 | -2,200 | 428,888 |
| muibase | 1,330,736 | 1,330,472 | -264 | 1,330,664 |
| homebank | 502,984 | 499,408 | -3,576 | 451,928 |
| cmanager | 748,712 | 746,928 | -1,784 | 697,560 |
| ro | 436,472 | 409,808 | -26,664 | 298,552 |
| multiren | 772,488 | 746,720 | -25,768 | ? |
| archiver | 378,488 | 368,848 | -9,640 | 244,456 |
| annotate | 883,808 | 883,808 | 0 | 883,808 |
| amigaampprefs | 346,008 | 325,936 | -20,072 | 213,112 |
| viewcsv | 334,640 | 324,952 | -9,688 | ? |
| todo | 132,168 | 131,192 | -976 | 124,448 |
| yam210 (mui5) | 3,897,864 | 3,805,256 | -92,608 | |
| dizzytorrent (mui5) | 2,517,440 | 2,510,232 | -7,208 | |
| amigagpt (mui5) | 2,072,568 | 2,055,136 | -17,432 | |

No figure: SimpleMail (wasabid dies at its quit), IBrowse 2.5.9 and 3.0a (FS-UAE dies at the second
quit, item 8 below), MQTTExplorer (ends at once); "?" for MUI 3.8: the second run did not quit or
quit late. 34 MUI 3.x programs together 212,584 bytes less (0.3-26.7 KB each), the three MUI 4/5
programs 117,248. InstallerNG, AmiTradeCenter and Annotate hold exactly the same on all three
(not looked into: perhaps their figure is taken before their MUI windows exist). Zune still holds more
than MUI 3.8 in most programs: 10-110 KB (RO 410 against 299 KB, AmFTP 406 against 308, AmigaAMP-Prefs
326 against 213, NewsCoaster 1,260 against 1,018): not traced yet, the next lever for phase 4's
follow-up (MUI-Demo's own objects are now below MUI 3.8's, so it is something these programs use
and MUI-Demo does not).

**Update, branch `realmem` (2026-11-06, phase 4 follow-up):** traced (docs/lean/README.md, "Real
programs' memory against MUI 3.8"). Most of the second-run gap is the measurement: **MUI 3.8 keeps
its pool puddles when a program quits** (AmFTP 112 KB, RO 80, AmigaAMP-Prefs 56 KB, freed only by a
flush) and the second run reuses them, while Zune gives everything back. The suite therefore also
records the first run from boot (seventh column). What Zune really spent more was fixed: an
event-node block for every shown object (now only for objects that take input, 80 instead of 120
bytes) and an image spec per object for the plain pen backgrounds (now shared). The whole suite on
this branch, `lean` and MUI 3.8 in the same session (results `/tmp/realmem/zwork/suite-real/fin3`,
`base3`, `mui38`, `fin5`, `base5`): MUI 3.x group 35 of 36 as `lean`'s run, 30 of 36 windows as on MUI
3.8 (the same six differ); the one other difference: ViewCSV's second quit reset the machine on
`lean` and on MUI 3.8 and not here (FS-UAE). MUI 4/5 group 5 of 5 as `lean`'s. Memory in use,
emulator figures, bytes ("1st": the first run from a fresh boot, library and classes included,
so Zune's larger code counts there):

| ID | lean | realmem | change | MUI 3.8 | lean 1st | realmem 1st | change | MUI 3.8 1st |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| nlist-demo | 311,728 | 302,336 | -9,392 | 262,080 | 806,240 | 797,448 | -8,792 | 716,200 |
| nlisttree-demo | 928,816 | 922,176 | -6,640 | 909,624 | 1,470,176 | 1,464,040 | -6,136 | 1,419,624 |
| texteditor-demo | 186,456 | 179,032 | -7,424 | 158,008 | 605,184 | 598,360 | -6,824 | 540,952 |
| thebar-demo1 | 208,160 | 202,880 | -5,280 | 194,376 | 592,312 | 587,584 | -4,728 | 542,688 |
| thebar-demo2 | 235,272 | 222,680 | -12,592 | 197,264 | 626,752 | 614,712 | -12,040 | 575,912 |
| thebar-demo3 | 243,456 | 230,744 | -12,712 | 207,080 | 632,896 | 620,736 | -12,160 | 583,664 |
| thebar-demo4 | 188,360 | 183,080 | -5,280 | 174,576 | 558,616 | 553,840 | -4,776 | 508,944 |
| thebar-demo5 | 155,696 | 151,344 | -4,352 | 145,304 | 525,952 | 522,056 | -3,896 | 479,672 |
| thebar-demo6 | 194,952 | 189,696 | -5,256 | 181,192 | 579,128 | 574,400 | -4,728 | 529,504 |
| thebar-demo7 | 189,464 | 186,872 | -2,592 | 185,600 | 619,680 | 617,640 | -2,040 | 545,776 |
| thebar-demo8 | 181,512 | 176,232 | -5,280 | 167,728 | 551,720 | 546,992 | -4,728 | 502,096 |
| thebar-demo9 | 278,016 | 271,640 | -6,376 | 266,192 | 709,544 | 703,720 | -5,824 | 631,584 |
| thebar-demo10 | 158,368 | 154,912 | -3,456 | 151,920 | 528,512 | 525,560 | -2,952 | 486,288 |
| textinput-demo | 123,064 | 117,088 | -5,976 | 107,768 | 749,240 | 743,816 | -5,424 | 701,208 |
| yam25 | 1,730,000 | 1,715,024 | -14,976 | 1,472,888 | 2,876,368 | 2,858,592 | -17,776 | 2,802,432 |
| muimine | 142,432 | 140,968 | -1,464 | 146,408 | 432,472 | 431,560 | -912 | 381,888 |
| installerng | 729,504 | 729,504 | +0 | 729,480 | 752,864 | 769,312 | +16,448 | 769,312 |
| amftp | 405,784 | 380,864 | -24,920 | 308,408 | 794,176 | 769,808 | -24,368 | 776,168 |
| atc | 513,360 | 513,360 | +0 | 513,360 | 811,808 | 812,312 | +504 | 767,848 |
| simplemail | ? | ? |  | ? | ? | ? |  | ? |
| newscoaster | 1,259,808 | 1,257,096 | -2,712 | 1,017,680 | 2,116,592 | 2,112,568 | -4,024 | 2,034,664 |
| ibrowse25 | ? | ? |  | 1,544,160 | 2,465,528 | 2,455,040 | -10,488 | 2,186,312 |
| amirc | 985,128 | 979,064 | -6,064 | 801,336 | 1,942,000 | 1,936,488 | -5,512 | 1,664,048 |
| wookiechat | 2,324,832 | 2,308,104 | -16,728 | 2,094,160 | 3,230,432 | 3,215,864 | -14,568 | 3,164,312 |
| amtelnet | 366,048 | 356,976 | -9,072 | 0 | 1,130,240 | 1,121,720 | -8,520 | 1,051,080 |
| scout | 469,384 | 464,024 | -5,360 | 428,912 | 1,005,136 | 1,000,376 | -4,760 | 664,264 |
| muibase | 1,330,472 | 1,329,456 | -1,016 | 1,330,664 | 2,158,288 | 2,157,824 | -464 | 2,067,592 |
| homebank | 499,408 | 487,576 | -11,832 | 451,928 | 1,088,544 | 1,077,264 | -11,280 | 1,023,536 |
| cmanager | 746,928 | 740,744 | -6,184 | 697,560 | 2,163,824 | 2,158,192 | -5,632 | 1,914,696 |
| ro | 409,808 | 376,048 | -33,760 | 298,600 | 701,000 | 667,816 | -33,184 | 601,536 |
| multiren | 746,696 | 742,336 | -4,360 | ? | 1,410,688 | 1,406,880 | -3,808 | 1,336,360 |
| archiver | 368,872 | 350,208 | -18,664 | 244,456 | 710,920 | 692,760 | -18,160 | 611,648 |
| annotate | 883,808 | 883,808 | +0 | 883,808 | 940,136 | 940,136 | +0 | 940,184 |
| amigaampprefs | 325,936 | 290,176 | -35,760 | 213,112 | 658,104 | 622,944 | -35,160 | 538,640 |
| viewcsv | ? | 319,440 |  | ? | 1,101,112 | 1,096,200 | -4,912 | 1,050,808 |
| todo | 131,192 | 127,568 | -3,624 | 124,448 | 406,520 | 403,472 | -3,048 | 363,968 |
| total (32 / 35 programs with all three) | 17,206,024 | 16,921,280 | -284,744 | 15,065,920 | 38,452,704 | 38,178,032 | -274,672 | 35,475,408 |

MUI 4/5 group (second run / first run): YAM 2.10dev 3,805,256 -> 3,582,416 / 5,038,440 -> 4,816,120,
DizzyTorrent 2,510,232 -> 2,498,568 / 2,852,424 -> 2,841,360, AmigaGPT 2,055,136 -> 2,044,080 /
3,172,464 -> 3,161,960; IBrowse 3.0a first run 2,755,264 -> 2,744,392. SimpleMail had no window
this session on all three (wasabid on FS-UAE's bsdsocket). InstallerNG's first-run figure moved by
16 KB with no change in its second run (it equals MUI 3.8's): not looked into.

**Update, branch `diffs` (2026-11-07, phase 6):** the remaining window differences worked through
(docs/lean/README.md, "The suite's last differences against MUI 3.8"): TheBar demo2's control window
and demo3 381 wide (were 399), AmIRC's server window 550x236 at 45,10 (was 256 high at y 0), AmTelnet
"AMTELNET.1 ... (77x24)" 640x256 at 0 (was "amtelnet.1 ... (77x23)", 248 high at y 4), IBrowse 2.5.9's
main window "[1] IBrowse: About IBrowse 2.5.9" 480x192 at 80,32 (none before): all as on MUI 3.8. The
whole MUI 3 group on the final library: **35 of 36 programs behave as on MUI 3.8** in windows and sizes
(the comparison script counts 32: YAM 2.5 shows its "Update notification" window on Zune since about
05:00 that day, also with the previous build of the branch, so YAM's own date-driven update check, not a
Zune difference; IBrowse's frozen second quit is FS-UAE's, 8 below; MultiRen differs only in MUI 3.8's
run being "still running after quit"); SimpleMail had no window (wasabid on FS-UAE's bsdsocket, run
three times this session, the same before these changes). MUI 4/5 group: 5 of 5 the same as `lean`'s run
the same day (windows and memory). Note: the `zune5` reference in `~/.cache/zune68-suite` is older than
`suitefix`; YAM 2.10's "Confirmation Request" 249x103 and AmigaGPT's 564x91 error already came from
`lean`. Results: `/tmp/diffs/zwork/suite-real/fin` (MUI 3 group), `s5` and `l5` (MUI 4/5, this branch
and `lean`).

**Update, branch `leftovers` (2026-11-10):** AmIRC's and AmTelnet's logos (Bitmap objects of the
programs' own 5-plane bitmaps) are drawn now (Bitmap's remap used a one-plane temporary row for
ReadPixelLine8); the ARexx port is made with the application (AMTELNET.1 and the others answer
`wasabi arexx PORT QUIT` as on MUI 3.8; before, a host's own message was replied unread). MUI 3 group
on the final library: **35 of 36 behave as on MUI 3.8** (the comparison script counts 32: SimpleMail had
no window, run twice, as before; IBrowse 2.5.9 frozen and AmTelnet reset at the second quit, FS-UAE's,
see below; MultiRen's MUI 3.8 run "still running after quit"); against `diffs`' run 34 the same, YAM 2.5
without its date-driven "Update notification" now. AmTelnet ("exited" once in the full run) and SimpleMail
were run again alone. MUI 4/5 group: 5 of 5 the same as `diffs`' (`s5`). Memory: first runs from boot
160-224 bytes more per program (the ARexx port; MUI 3.8 makes it too), second runs within 112 bytes.
Seen, not changed: AmTelnet's empty Server string shows its cursor on Zune, not on MUI 3.8. Results:
`/tmp/leftovers/zwork/suite-real/fm3` (MUI 3 group) and `fm5` (MUI 4/5).

**Update, branch `mui45` (2026-11-10, phase 6, MUI 4/5 side):** the MUI 4/5 group widened to 24
entries (above) and baselined on MUI 5 and on Zune with Automatic and with MUI 5 forced; the
whole table, what was fixed and what is left in docs/lean/README.md ("MUI 4/5 programs: a wider
test set"). In short: on Zune before, MUI 5's Hyperlink, GIFAnim and LayGroup demos ended at once
and EmbeddedImages could not unpack its "Z" pictures, ElidedText opened cut, the TabGroup demo
showed empty tab groups (a MUI 3.8 bug in Group: fixed), Pointers fitted MUI 3.8's way where MUI 5
scrolls; now all open, and with MUI 5 forced Pointers and EmbeddedImages get MUI 5's scrollable
window. LumiZip resets the machine at quit on Zune with the MUI 3.8 identity only (open). The MUI 3
group with the branch's library: 35 of 36 as `lean`'s run (SimpleMail windowless as always), no
MUI 4/5 file loaded (LoadSeg trace), memory the same (runs without a trace attached: `wasabi
snoop` running during a run adds to the in-use figures, AmigaGPT +110 KB). The suite now waits 150 s
for EmbeddedImages and finds its window by title (its task name is longer than `wasabi` shows).
Results: `/tmp/mui45/zwork/suite-real/` m5a/m5b (MUI 5), za/zb and z5a/z5b (Zune before, Automatic
and MUI 5), fa/fb and z5f (after), n3a/n3b and f3a/f3b (MUI 3 group, with `.loadseg` traces).

**Update, branch `mui45b` (2026-11-11, phase 6):** LumiZip's reset at quit is fixed: LumiZip
calls MUI_CreateCustomClass with a supername and garbage as supermcc; MUI 3.8 and MUI 5 take the
name (tests/lean/ccprio.c), Zune took the garbage and closed it as a library when the class was
deleted at quit. Found with a start/quit loop on one boot (`trapcap` caught it within 1-11 rounds;
the suite reboots between programs, so it saw only two quits per run). With the fix: 26 of 26
quits clean. Also added: MUIM_FindObject (IBrowse 3.0a), MUIA_FrameVisible/FrameDynamic (YAM
2.10), List's sort mark (MUI 5 List1), Title's register tabs (MUI 5 Title). Details and the
results in docs/lean/README.md ("LumiZip's reset found").

**Update, branch `amiberry` (2026-11-11): the suite on Amiberry.** Every run above was FS-UAE's;
this round moved the suite onto Amiberry 8.5 headless (docs/lean/README.md, "Testing on
Amiberry") and ran all of it there, real MUI 3.8 and MUI 5 included, so that the emulator's own
failures could be told from OpenMUI's. **MUI 3 group on Amiberry: 35 of 36 as on MUI 3.8** (the one:
MultiRen's run on MUI 3.8 is "still running after quit", as always); against the same library's
FS-UAE run 33 of 36 the same, the three being the emulators': InstallerNG freezes Amiberry at its
start (Zune and MUI 3.8 alike, see the traps), IBrowse 2.5.9's second quit is clean on Amiberry
(-64 bytes; FS-UAE dies there), AmTelnet does not start from Amiberry's drive (now started from
RAM: by `RAMRUN[amtelnet]`: ok, -8,192 on MUI 3.8 as on FS-UAE). MUI 3.8 on Amiberry against MUI 3.8
on FS-UAE: 31 of 36 the same; the five are FS-UAE's frozen second quit of YAM 2.5, its reset at
ViewCSV's second quit, InstallerNG, AmTelnet, and SimpleMail. **MUI 4/5 group: 22 of 24 as the
FS-UAE run** (YAM 2.10 and IBrowse 3.0a, below); against real MUI 5 on Amiberry the same window
differences as before, and two that FS-UAE had hidden: MUI 5's own EmbeddedImages demo opens on
real MUI 5 (640x256, scrollable; FS-UAE's MUI 5 had "exited") and OpenMUI gave it no window, its
Clock demo opens on MUI 5 (FS-UAE: "nowindow,busy"). Found and fixed on the way:

- **SimpleMail's "no window" was the suite's** (every round since 2026-11-06, both emulators):
  `wasabi ps` cuts a task name at about 34 characters, so `running()` never saw
  `Workbench:Real/simplemail/SimpleMa` and gave up 2 s after the start; its window opens within
  5 s on both emulators and both MUIs. What is real on both: a `wasabi run` while SimpleMail runs
  ends wasabid (the suite's `avail` does), and its quit resets the machine, so there is still no
  memory figure; the windows are recorded before the memory check now.
- **YAM 2.10's "Error messages" window that would not close**: YAM's own "Invalid configuration"
  request (a MUI request: the application asleep, its menus gone) was up underneath; on MUI 3.8
  the same (by hand, Amiberry). On Amiberry the fifty errors come faster than the suite closed the
  window, so the request was always up first. The hook answers it (Ok); YAM 2.10 now quits on
  OpenMUI with its main window listed ("YAM 2.10-dev - User:" 640x256, -336 bytes).
- **The "frozen after the first window" path died** on an unbound `wl` under `set -u`; FS-UAE had
  hardly ever taken it.
- **OpenMUI: a window larger than the screen is scrollable for a MUI 4 program too** (Window):
  MUI 5's EmbeddedImages and Pointers demos ask for muimaster version 20 (MUI 4 under Automatic)
  and open scrollable on real MUI 5; EmbeddedImages now opens 640x256 as there (was: no window
  at all, the demo waiting for ever), Pointers 640x246 (was 364x248; MUI 5 640x256). MUI 3.8
  programs are not touched (the same branch of the fitting loop, their condition unchanged).

**The final library of the branch, whole groups again** (Amiberry): MUI 3 group 34 of 36 as the
first run (SimpleMail and AmTelnet now with their windows, the fixes above), against MUI 3.8
(`ab-m38c`) 35 of 36 but for SimpleMail's MUI 3.8 record from before the record fix: only MultiRen
differs, as always; TheBar demo3 was "frozen" once while 16 capacity instances loaded the PC and
ok run again alone. MUI 4/5 group 21 of 24 as the first run, the three being the improvements
(YAM 2.10 quits with its main window, EmbeddedImages opens, Pointers 640x246); against MUI 5 the
window-size differences only. Results `ab-z3f`, `ab-z5f`.

Results: `/tmp/amiberry/zwork/suite-real/ab-z3` (OpenMUI, MUI 3 group), `ab-m38` (MUI 3.8),
`ab-z5` / `ab-z5b` (OpenMUI, MUI 4/5 group, before / after the Window change), `ab-m5` (MUI 5),
`ab-z3c` / `ab-m38c` (SimpleMail and AmTelnet again with the fixes); contact sheets
`compare-ab-z3-ab-m38.html`, `compare-ab-z5-ab-m5.html`, `/tmp/amiberry/cmp-*-fsuae.html`.

**Update, branch `compat1` (2026-11-11, compatibility round 1):** 48 more programs (group `mui3`)
and a group `mcc` of 49 add-on class demos (the tables above), all baselined on MUI 3.8, MUI 5 and
OpenMUI (results `/tmp/compat1/zwork/suite-real/r1-new-*`, `r1-mcc-*`; the whole suite on the final
library `fin`; docs/lean/README.md, "Compatibility round 1"). New programs, OpenMUI against MUI
3.8: **36 of 48 the same**; OpenMUI's: Voyager 3.5.3 resets the machine at its second quit (a quit
by its close gadget leaves exec's memory list insane; ARexx QUIT is clean), NFS-GUI 1.1 ends with
return code 20 before its window, MicroDot-II ends at once in two runs of three (ok with its
database requester in the third; ok on MUI 3.8, ends at once on MUI 5), and four windows a few
pixels off (TinyMeterPrefs, NewsRog, the three Lumi programs, u64mui); MUI 3.8's or the
emulator's: iGame frozen, AHI's preferences reset, AmiDrop still running after quit, each once.
Class demos: 37 of 49 the same, **39 after the fix** (Window's OM_NEW no longer runs the
subclass's OM_SET: BWin.mcc's demo1 and demo3 open); left: Lamp's demo 346x227 against 640x256
(Coloradjust's default size), the TipOfDay demo's title and list scroller, TearOff's second
window 326 against 256 wide, Busy 2.4's demo on OpenMUI's own Busy.mcc (136x130 against
148x151), HexEdit, TableGroup, HTMLtext, GIFAnim and SpeedBar demo6 within 1-58 pixels, and the
HTMLview demo still running after the suite's quit. Thirteen demos fail on all three targets
alike (the demo's: an argument, a file, a library, OS 3.1's missing PNG datatype). The MUI 3
group of before: 35 of 36 as the previous run (AmiTradeCenter opens its Server Manager instead
of its toolbar this round, with `lean`'s window.c too: the widened install, open); the MUI 4/5
group 24 of 24 as before. Which MUI each new program asks for is in its table row ("asks").

**The "lost" column's sign** (found 2026-11-06): `avail`'s third column on the "total" line is
In-Use (checked: "Type Available In-Use Maximum Largest"), so "lost" is in-use before the second
run minus in-use after it: a *negative* figure is memory the second run left in use, a positive one
memory it gave back. The text above (and in earlier updates) read it the other way round: TheBar
demo1's -27,456 bytes on Zune and -27,432 on MUI 3.8 are 27 KB still in use after its second run on
both (TheBar.mcc's caches, as the equal figures suggest), YAM 2.5's +63,280 on MUI 3.8 is memory the
second run gave back.

**AmTelnet's reset at its second quit is FS-UAE's, not Zune's** (2026-11-06, branch `smaller`):
AmTelnet started and quit 3-10 times per boot, with a marker file in RAM: to tell a reboot from a
stalled wasabid. FS-UAE with Zune, ~175 cycles: 2 resets, 1 freeze and 1 start without a window,
all but one at the second cycle after boot. FS-UAE with MUI 3.8, 113 cycles: 1 reboot during the
second cycle, 1 freeze and 2 starts without a window. Amiberry 8.5.0 headless with Zune, 100
cycles: none. The same thing happens with MUI 3.8 and never in Amiberry, so it is FS-UAE (likely
its emulated bsdsocket.library, which AmTelnet opens and closes on every run) with AmTelnet. In
Amiberry, AmTelnet has to be started from RAM: (from Amiberry's host-directory drive it fails with
"object is not of required type" when it reloads itself).

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
   clipped at 640 pixels, or not at all. **Fixed (suitefix)**, IBrowse too since `diffs` (its toolbar read the preset fonts from the render info): MUI 3.8 has five
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
   **Run in Amiberry 2026-11-06: FS-UAE's fault, not Zune's.** The same drive (the suite's
   installed copy of a1200-31 with Zune at `demopix`'s start, IBrowse 3.0a and 2.5.9 and their
   classes) in Amiberry 8.5.0 (A1200, Kickstart 3.1, 68020 + 68882, 2 MB chip + 8 MB fast,
   directory drive, bsdsocket emulation, headless: `SDL_VIDEO_DRIVER=offscreen`, no display), run
   by `suite.sh` through an `amiga-test` stand-in: 4 of 4 runs of both programs, two starts and
   quits each, ok; IBrowse 3.0a leaves 64 bytes on the second run (MUI 5: 4,568), 2.5.9 0.
   The same suite run in FS-UAE died in the first run of IBrowse 3.0a and of 2.5.9 ("malloc():
   unaligned tcache chunk detected", the emulator aborting, not the Amiga). IBrowse 2.5.9 still
   opens no main window on Zune (the window fitting, 1 above) in both emulators. A run where two
   suite runs drove the same Amiberry instance by mistake reset the machine; with one driver it
   never did.
9. **Window sizes** (each a few pixels; the contact sheet shows where). **Mostly fixed
   (suitefix)**: all below match MUI 3.8 now except TheBar demo2's control window and demo3
   (18 px wider; **fixed in `diffs`** with AmTelnet and AmIRC), AmTelnet, NListtree's tree images and the MUI 4/5 group (not looked at);
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

Amiberry (2026-11-11; the runner's own traps are in `scripts/amiberry-test`'s header and
~/Projects/Knowledge/amiga/amiberry-headless.md):

- **InstallerNG 1.5 freezes the whole machine at its start in Amiberry** (OS 3.1, 68020; the .020
  and .debug builds freeze, the .000 build resets; with or without FPU, `cpu_compatible=true`,
  `cpu_speed=real`; a DOS script in another process stops too), with OpenMUI and with MUI 3.8.
  FS-UAE runs it. The suite marks it "frozen" there (six minutes of waiting); run `installerng`
  with `EMU=fsuae`.
- **AmTelnet II does not start from Amiberry's host-directory drive**, x bit or not: it opens its
  own file and its `RunCommand` of itself returns -1 (snooped). `RAMRUN[amtelnet]` copies it to
  RAM: first.
- A `wasabi run` while SimpleMail runs ends wasabid, and SimpleMail's quit resets the machine,
  on Amiberry as on FS-UAE: SimpleMail's own doing.
- YAM 2.10's "Invalid configuration" request is up before its error window can be closed on
  Amiberry (the errors come faster): answer the request first (the hook does).
- `wasabi ps` cuts long task names: match a prefix, or give the program a TITLE.

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
- A `wasabi snoop` attached to the instance (`snooplog.sh`, the LoadSeg trace) makes YAM 2.10 on
  the zune target answer nothing for minutes: OS 3.1 has no PNG datatype, YAM tries its 50 theme
  pictures through datatypes and every DOS call is traced. Its "Error messages" window then stays
  (neither its close gadget nor its Close window button does anything), YAM does not quit and the
  second run is lost (2026-11-11, branch `listslim`: yam210 alone with a snoop three times, the
  window every time; without, never). Trace the MUI 4/5 group without yam210. The dismiss hook
  ends only after two quiet checks, as the window may come late.
