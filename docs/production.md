# OpenMUI 1.0: what production grade means

Decided with our partner on 2026-11-11: OpenMUI must be excellent, nothing less. This page turns that
into gates that can be checked. OpenMUI 1.0 ships when every gate is green; until then releases are
`0.xbN` betas. Each gate names how it is measured. Emulator figures are labelled as such.

## 1. Compatibility (the first goal)

| Gate | Measured by | Today |
|---|---|---|
| Every MUI 3 program in the suite that runs on real MUI 3.8 runs on OpenMUI, same windows and sizes | `suite.sh` group mui3 against MUI 3.8 (Amiberry) | as round 2 (35 of 36 old, 36 of 48 new alike) plus 14 of 16 new alike |
| Every MUI 4/5 program in the suite behaves as on real MUI 5 | group mui5 against MUI 5 | 24 of 24 status (one freeze under load rerun), known size differences |
| Every add-on class demo that runs on MUI 3.8 runs on OpenMUI | group mcc against MUI 3.8 | 40 of 49 alike (Busy demo now), 9 of 11 new alike |
| MUI's own demos pixel-identical to MUI 3.8 at default settings | `demopix.sh` | 19 of 27 |
| No known compatibility bug open without a decision | roadmap "Bugs found from outside", README "Left open" | open with a decision: Voyager (its own blit, freed too early; a lead kept); NFS-GUI 1 px, TinyMeterPrefs default height |
| The suite covers what people use: at least 120 programs and 60 class demos | `real-programs.md` tables | 132 programs, 60 demos (2026-11-12 02:00-02:20, Amiberry, nine instances side by side) |

## 2. Stability

| Gate | Measured by | Today |
|---|---|---|
| No crash, alert or hang caused by OpenMUI in the whole suite | suite statuses, trapcap, alerts | Voyager close-quit reset: Voyager's own (no WaitBlit before its FreeBitMap), reproducible only on OpenMUI |
| No memory leak: every program gives back what it took; hours of open/close loops flat | fullcheck "lost", a soak test (to build) | fullcheck flat; no soak test yet |
| Out of memory never crashes: every allocation failure handled | fault injection at random allocations (to build) | not tested |
| Works on a stock 68000, OS 3.1, 2 MB | the a600-31 test machine running the demos and a suite subset | not run |
| Works on a graphics-card screen | the suite on an RTG test machine (FS-UAE uaegfx) | single tests only |
| Robust input: ARexx commands, prefs files, image specs, format strings cannot crash it | a fuzz run over the parsers (to build) | not tested |

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
