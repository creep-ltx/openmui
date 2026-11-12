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
| No crash, alert or hang caused by OpenMUI in the whole suite | suite statuses, trapcap, alerts | Voyager close-quit reset: Voyager's own (no WaitBlit before its FreeBitMap), reproducible only on OpenMUI; stability round 1 (2026-11-12): the 181 run again on the fixed build, see docs/lean/README.md "Stability round 1" |
| No memory leak: every program gives back what it took; hours of open/close loops flat | fullcheck "lost", `soak.sh` (built 2026-11-12), `suite.sh --loops K` (built) | fullcheck flat (A1200 and A600); soak 70 min: windows constant, in use +6 KB over 52 cycles (about 120 bytes a cycle, not yet attributed), the loops not yet run over the suite |
| Out of memory never crashes: every allocation failure handled | `failn` + `failsweep.sh`: the Nth resource call from library code fails, swept over a program's whole start and quit (built 2026-11-12) | MUI-Demo, all 2,175 calls: no crash of OpenMUI's (the two resets were dos.library's own OpenLibrary inside Open()); children of a failed Window/Application were kept (8-27 KB / 102 KB): fixed by `zune_dispose_children`; left: intuition's own NewObjectA failing (MUI 3.8 keeps the same there). Other programs not yet swept |
| Works on a stock 68000, OS 3.1, 2 MB | `smallcheck.sh` on a600-31 with 0/1/2 MB fast (built 2026-11-12) | fullcheck's 19 demos and MUI-Demo's eight sub-windows run and quit with 0, 1 and 2 MB fast, lost figures as the A1200; the suite subset not yet |
| Works on a graphics-card screen | the suite on an RTG test machine (FS-UAE uaegfx) | single tests only |
| Robust input: ARexx commands, prefs files, image specs, format strings cannot crash it | `check_fuzz_parsers.py` and `check_mui38_fuzz.py` in `make check`, `prefsfuzz.sh`, `rexxfuzz.sh` (built 2026-11-12) | parsers 30,000 inputs no overread/leak; importer 5,500 files under ASAN clean; ARexx 314 commands no crash; settings files: no reset, every file starts the program or is refused (first 250 seeds); List format strings not yet fuzzed (ReadArgs-based) |

## 3. Memory and speed

| Gate | Measured by | Today (emulator) |
|---|---|---|
| Each further program uses no more memory than on MUI 3.8 | `memtally`, suite memory columns | 204 KB vs 211-219 KB |
| First program within MUI 3.8's figure | MUI-Demo cold | 474 KB vs 460 KB |
| Every common operation at least as fast as MUI 3.8 | per-operation timing against MUI 3.8, cycle-exact FS-UAE | not measured per operation |
| Speed confirmed on real hardware | our partner's spare A1200 | waiting for the machine |

## 4. Real use

| Gate | Measured by | Today |
|---|---|---|
| Daily use on our partner's A1200 for weeks without an OpenMUI problem | their reports, the crash recorder | not installed yet |
| Public betas on Aminet with feedback handled | issue tracker on GitHub | no public beta yet |

## 5. Engineering

| Gate | Measured by | Today |
|---|---|---|
| Every change builds and passes `make check` in CI | GitHub Actions on creep-ltx/openmui | yes |
| A code audit of the whole library for memory safety and error paths, findings fixed | a dedicated review round | not done |
| Clean-room record: MUI studied only black-box | docs, commit messages | kept |
| No compiler warnings in the library | build with -Wall -Wextra | to check |

## 6. Documentation and release

| Gate | Measured by | Today |
|---|---|---|
| A user guide: install, switch, settings, "Behave as", uninstall, troubleshooting | docs/ | README and readme only |
| A developer guide: what OpenMUI supports, differences from MUI, the SDK | docs/ | not written |
| A changelog per release | CHANGELOG.md | not yet |
| Translations of OpenMUI's own texts (Swedish first) | catalogs | English only |
| A release process: version, archive, Aminet readme within limits, tag | tools/release.py, CI | archive yes, tag/CI for Aminet to update |

## Order of work

1. Compatibility rounds until gate 1 is green (running now).
2. Stability: the soak test, fault injection, the 68000 and RTG runs, the parser fuzzing, the code audit.
3. Memory and speed: per-operation timing against MUI 3.8, fixes.
4. Documentation, changelog, translations, release process.
5. Public beta on Aminet, then daily use and real-hardware speed, then 1.0.
