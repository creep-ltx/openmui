# Roadmap of the lean edition

Decided with our partner on 2026-11-05. The direction: MUI 3.8's look,
memory and speed (better where we can), MUI 3, 4 and 5 programs running.
MUI 3.8 is studied black-box or from its documentation, never from its
code. One phase at a time; each phase ends measured, tested (make check,
fullcheck, the real-program suite) and merged.

Measured on 2026-11-05 (FS-UAE A1200, OS 3.1, 4 MB fast RAM, MUI-Demo):

| | lean | MUI 3.8 |
|---|---|---|
| library code loaded at start | 261 KB | 166 KB |
| first program | ~497 KB | 449-460 KB |
| each further program | ~222 KB | 206-219 KB |
| first window | 1.5 s | 1.46-1.62 s |

## Phases, in order

1. **Real-program suite.** More real MUI programs, chosen by what people
   use (mail, web, IRC, FTP, databases, tools), MUI 3.x ones first, then
   MUI 4/5-only ones. Each: install, start, open its main windows, quit;
   screenshots and memory compared with MUI 3.8 (and MUI 5 for 4/5-only
   programs). Runs from one script. Everything after this is checked
   with it.
2. **Demo set compared with MUI 3.8 automatically.** The 19 MUI 3.8
   demos and MUI-Demo's sub-windows, screenshotted on Zune and MUI 3.8
   with the same settings, compared pixel by pixel; a report of every
   difference. The goal is pixel-identical at default settings.
3. **Smaller library** (target: code loaded at start <= 166 KB). More
   rarely used code into files loaded on demand (window snapshot and
   title gadgets, list editing and drag, string extras, ...), then the
   largest classes made smaller where they do work MUI 3.8 does not.
4. **Memory per program** (target: first program <= 460 KB, each
   further <= 210 KB): window backing store, list pools, notifications,
   Text contents.
5. **Speed** (target: faster than MUI 3.8): start-up, window opening,
   redraw, list scrolling; measured on real 68000/68020 hardware, not
   only in the emulator, and never PiStorm figures as Amiga speeds.
6. **MUI 3 behaviour exactly as MUI 3.8** and **MUI 4/5 programs run**:
   whatever the suite and the demo comparison find, fixed in order of
   how many programs it affects; MUI 4/5 features only as files loaded
   on demand, so MUI 3 programs pay nothing for them.
7. **Settings program as complete as MUI 3.8's**, including reading
   existing MUI 3.8 settings files (their item ids are known for the
   title gadgets; the rest to be measured).

## Later (wanted, not scheduled)

1. Hand-written 68000 assembly in the hottest drawing and layout loops.
2. The remaining AROS-derived code rewritten smaller, piece by piece.
3. MUI 5's extra classes and features as optional files.
4. Faster drawing on graphics cards (RTG).
