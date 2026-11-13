# OpenMUI 1.0: what production grade means

Decided with our partner on 2026-11-11: OpenMUI must be excellent, nothing less. This page turns that
into gates that can be checked. OpenMUI 1.0 ships when every gate is green; until then releases are
`0.xbN` betas. Each gate names how it is measured. Emulator figures are labelled as such.

## 1. Compatibility (the first goal)

| Gate | Measured by | Today |
|---|---|---|
| Every MUI 3 program in the suite that runs on real MUI 3.8 runs on OpenMUI, same windows and sizes | `suite.sh` group mui3 against MUI 3.8 (Amiberry) | as round 2 (35 of 36 old, 36 of 48 new alike) plus 14 of 16 new alike |
| Every MUI 4/5 program in the suite behaves as on real MUI 5 | group mui5 against MUI 5 | 24 of 24 status, known size differences; MUI 4/5 programs now get MUI 5's bar titles, fonts, List scroller and Busy size (round 4), but most of them ask for version 19 and so get MUI 3.8 under Automatic (AmigaGPT) |
| Every add-on class demo that runs on MUI 3.8 runs on OpenMUI | group mcc against MUI 3.8 | 40 of 49 alike (Busy demo now), 9 of 11 new alike |
| MUI's own demos pixel-identical to MUI 3.8 at default settings | `demopix.sh` | 19 of 27 (17 in round 4's run: ShowHide and InputHandler on MUI 3.8's side, OpenMUI's shots byte-identical to round 3's) |
| No known compatibility bug open without a decision | roadmap "Bugs found from outside", README "Left open" | open with a decision: Voyager (its own blit, freed too early; its own set, decided by its content List); the group spare-pixel rule (measured, not found), NFS-GUI 1 px, TinyMeterPrefs default height; found and fixed in round 4: a program's identity lost after another program had used a class library (roadmap bug 4) |
| The suite covers what people use: at least 120 programs and 60 class demos | `real-programs.md` tables | 132 programs, 60 demos (2026-11-12 03:33-04:26, round 4's final run: 173 of 181 as round 3, the rest reruns or MUI 5-ward sizes) |

## 2. Stability

| Gate | Measured by | Today |
|---|---|---|
| No crash, alert or hang caused by OpenMUI in the whole suite | suite statuses, trapcap, alerts | Voyager close-quit reset: Voyager's own (no WaitBlit before its FreeBitMap), reproducible only on OpenMUI; stability round 2 (2026-11-12): the 181 with 5 start/quit loops each (705 cycles), no quit crash of OpenMUI's; the 181 again on the fixed build: as lean, program by program (three "frozen" under load ran as lean alone) (docs/lean/README.md "Stability round 2"); AnimMaker's resets (`animfix`): AnimMaker's own, 6.2 % of boots on OpenMUI and 6.4 % on MUI 3.8 |
| No memory leak: every program gives back what it took; hours of open/close loops flat | fullcheck "lost", `soak.sh`, `suite.sh --loops K` (flushed on OpenMUI since round 2), `runkeep.sh` (memtally per run) | fullcheck flat (A1200 and A600); soak 70 min flat, lost 0; loops over the 181 (round 2): unflushed growth the same as MUI 3.8's (TheBar demo1 137,160 bytes on both), flushed 0 by the third run for the ten largest; ViewCSV keeps about 3 KB a run on both (OpenMUI flushed 2,952 bytes a run, MUI 3.8 unflushed 17,488 over 5): objects it creates each run, recorded |
| Out of memory never crashes: every allocation failure handled | `failn` + `failsweep.sh`, `failreal.sh` for real programs (round 2): the Nth resource call from library code fails | MUI-Demo all 2,175 calls (round 1); round 2 about 1,000 sampled N's over 10 programs and demos: two OpenMUI resets fixed (the next flush after a class file could not be loaded; a double dispose of a Popstring's or Listview's parts), the library no longer stays unexpungeable after OS 3.1's MakeClass failed inside (worked around) or Popasl found no asl.library (fixed); the other resets the programs' own (HomeBank, Annotate); left: Popup reset once at one N with the trap recorder off, not again with it on; 3 resets without a record |
| Works on a stock 68000, OS 3.1, 2 MB | `smallcheck.sh` on a600-31 with 0/1/2 MB fast (built 2026-11-12) | fullcheck's 19 demos and MUI-Demo's eight sub-windows run and quit with 0, 1 and 2 MB fast, lost figures as the A1200; the suite subset not yet |
| Works on a graphics-card screen | the suite on an RTG test machine (FS-UAE uaegfx) | round 2: 20 suite programs by hand on a 1024x1024 24-bit Picasso96 screen (OS 3.2.1, 68030, FS-UAE): 19 opened and drew right, none crashed (AmFTP's window is another process's); the suite tool itself is too slow on FS-UAE's 68030 under load |
| Robust input: ARexx commands, prefs files, image specs, format strings cannot crash it | `check_fuzz_parsers.py` and `check_mui38_fuzz.py` in `make check`, `prefsfuzz.sh`, `rexxfuzz.sh` (built 2026-11-12) | parsers 30,000 inputs no overread/leak; importer 5,500 files under ASAN clean; ARexx 314 commands no crash; settings files: no reset, every file starts the program or is refused (first 250 seeds); List format strings not yet fuzzed (ReadArgs-based) |

## 3. Memory and speed

| Gate | Measured by | Today (emulator) |
|---|---|---|
| Each further program uses no more memory than on MUI 3.8 | `memtally`, suite memory columns | 193.6 KB vs 210.6-218.9 KB (rw-group 2026-11-13, FS-UAE; draw1 196.7 KB, speed round 3: 204.5 KB) |
| First program within MUI 3.8's figure | MUI-Demo cold | yes: 444,592 vs 460,296 bytes (rw-group, 2026-11-13: Group's instance data 68 -> 36 bytes and 3 KB less code); draw1: 450,872 (2026-11-13; lean after the List and Window rewrites 448,488: draw1's fixes cost 2.4 KB of code). Speed round 3: 459,992 vs 460,296 bytes (speed round 3, same session; round 2 460,208): a margin of 304 bytes. Round 2 moved MUI 4/5-only and AROS-only code into the Zmui45 module (pointer shapes, Text shortening and marking, RGB text pens, MUIO_ImageButton); round 3's tag walk in the library's own code made the code 216 bytes smaller |
| Every common operation at least as fast as MUI 3.8 | `opbench` (27 operations, the same binary on both), cycle-exact FS-UAE, two instances each, swapped (built 2026-11-12) | rw-group (2026-11-13): A1200 full redraw 140.9-141.0 ms against MUI 3.8's 140.6-150.9 (lean 148.2), creation 0.91-0.93, relayout 0.87-0.91; 68000 (a600-31 + 4 MB fast): full redraw 477.6-477.7 against 470.2-478.1 (1.00-1.02; lean 1.07), creation 0.95-1.01 (lean 1.00-1.04), relayout 0.92, list insert 1 within MUI 3.8's 1-3 % as lean; iconify bimodal (frame waits). Before, draw1 (2026-11-13): open+close 0.79-0.81, uniconify 0.79-0.80, register page 0.85, popup 0.81, relayout 0.93-0.98, full redraw 0.98-1.06 against MUI 3.8's faster and slower boots, resize at the OS's own floor (218.7 ms without MUI, MUI 3.8 220-230, OpenMUI 220.1); all 26 at least as fast as two of three MUI 3.8 instances, the full redraw 1.06 against the third (its faster boot, 140.6 ms). 68000 (a600-31 + 4 MB fast, MUI 3.8 copied on): faster in most, slower in the full redraw (1.06-1.08), creation (1.00-1.04) and a list insert (1.03). Earlier (speed round 3): button click 0.78, slider set 0.67, list sort 0.17 |
| Every common operation faster than MUI 5 for MUI 4/5 programs | `opbench v21` on a1200-32-mui5 (OS 3.2.1): real MUI 5 21.227 against OpenMUI in its place, opened as version 21 (MUI 5 identity); two instances, swapped (built 2026-11-12) | speed round 3: faster in all 27 (26 + MUI 5's "text shorten set"): 0.87 (Cycle set) to 0.02 (list insert 1000); window open+close 0.46, button click 0.83-0.84, typing 0.49, uniconify 0.34, popup 0.22-0.23 |
| Speed confirmed on real hardware | our partner's spare A1200 | waiting for the machine |

## 4. Real use

| Gate | Measured by | Today |
|---|---|---|
| Daily use on our partner's A1200 for weeks without an OpenMUI problem | their reports, the crash recorder | in daily use since 2026-11-06 (Zune68 before that) on their A1200 (PiStorm, Emu68), the only MUI on it; the 2026-11-06 build, today's not yet installed |
| Public betas on Aminet with feedback handled | issue tracker on GitHub | no public beta yet |

## 5. Engineering

| Gate | Measured by | Today |
|---|---|---|
| Every change builds and passes `make check` in CI | GitHub Actions on creep-ltx/openmui | yes |
| A code audit of the whole library for memory safety and error paths, findings fixed | a dedicated review round | round 2 (2026-11-12): every string and memory copy read, allocation results scanned, dispose and set paths, sizes: 6 found and fixed (Configdata, PanelTitle, Gauge, List size, Boopsi get, Popasl parts) besides the sweeps'; one left by the stack rule (Popasl NULL contents) |
| Clean-room record: MUI studied only black-box | docs, commit messages | kept |
| No compiler warnings in the library | build with -Wall -Wextra | -Wall: 0 in muimaster.library and its class files (round 2, was 4); the bundled third-party classes keep their 29; -Wextra: about 800, nearly all unused parameters and sign comparisons (the buffer ones read, none real), not switched on |

## 6. Documentation and release

| Gate | Measured by | Today |
|---|---|---|
| A user guide: install, switch, settings, "Behave as", uninstall, troubleshooting | docs/ | `docs/user-guide.md` written 2026-11-12 (install, switch, uninstall, prefs, "Behave as" and its file, LIBS:OpenMUI (named so since 2026-11-12, with the update from LIBS:Zune / ENV(ARC):Zune), troubleshooting, reporting); to be checked on a real install |
| A developer guide: what OpenMUI supports, differences from MUI, the SDK | docs/ | `docs/developer-guide.md` written 2026-11-12 (support, identity, known differences, build, tests, clean room, contributing); the SDK reference is not written |
| A changelog per release | CHANGELOG.md | `CHANGELOG.md` has the 0.1b1 entry and an empty Unreleased section (2026-11-12) |
| Translations of OpenMUI's own texts (Swedish first) | catalogs | Swedish was already in the package, from the AROS translation team (2026-11-12 audit): the library 57 of 57 ids, prefs program 220 of 220 (228 with the MUI-version texts added 2026-11-12, Swedish included), PrefsWindow 5/5, AboutWindow 5/5, SystemPrefsWindow 12/12, plus 5 bundled-class `.po` files; installed as `Catalogs/svenska`. A release check now guards it. The five catalogs were vendored into the repository (2026-11-12, `UPSTREAM.txt` in each folder gives the AROS translation team source and commit) so OpenMUI can fix them; Swedish fixes made (Color, Mark, Top/Bottom, the tape labels). The prefs page's "MUI version" group and OpenMUISwitch (new `OpenMUISwitch.catalog`, 36 ids, Swedish) now use catalogs, English built in. Our partner (Swedish) approved the Swedish texts 2026-11-12. Left to do: the other languages for the new strings (they fall back to English), and a real-Amiga test of the Swedish locale |
| A release process: version, archive, Aminet readme within limits, tag | tools/release.py, CI | archive yes; the CI and Aminet workflows name OpenMUI (2026-11-12, `aminet-release.yml` untested until a tag is pushed); no tag yet; the Aminet pair is `OpenMUI.lha` / `OpenMUI.readme` (decided 2026-11-12; `prepare_aminet.py` and `aminet-release.yml` make and upload them since 2026-11-12, checked in `check_release.py`) |

## Order of work

1. Compatibility rounds until gate 1 is green (running now).
2. Stability: the soak test, fault injection, the 68000 and RTG runs, the parser fuzzing, the code audit.
3. Memory and speed: per-operation timing against MUI 3.8, fixes.
4. Documentation, changelog, translations, release process.
5. Public beta on Aminet, then daily use and real-hardware speed, then 1.0.
