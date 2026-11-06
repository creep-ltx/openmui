# Roadmap of the lean edition

Decided with our partner on 2026-11-05. The direction: MUI 3.8's look,
memory and speed (better where we can), MUI 3, 4 and 5 programs running.
MUI 3.8 is studied black-box or from its documentation, never from its
code. One phase at a time; each phase ends measured, tested (make check,
fullcheck, the real-program suite) and merged.

Measured on 2026-11-05 (FS-UAE A1200, OS 3.1, 4 MB fast RAM, MUI-Demo):

| | lean | MUI 3.8 |
|---|---|---|
| library code loaded at start | 261 KB (2026-11-06, `smaller`: 246 KB, file 262 KB) | 166 KB |
| first program | ~497 KB (2026-11-06, `memory`: 479,064 bytes) | 449-460 KB (460,296 bytes) |
| each further program | ~222 KB (2026-11-06, `memory`: 207,704-207,768 bytes) | 206-219 KB (210,696-218,824 bytes) |
| first window | 1.5 s | 1.46-1.62 s |

## Phases, in order

1. **Real-program suite.** More real MUI programs, chosen by what people
   use (mail, web, IRC, FTP, databases, tools), MUI 3.x ones first, then
   MUI 4/5-only ones. Each: install, start, open its main windows, quit;
   screenshots and memory compared with MUI 3.8 (and MUI 5 for 4/5-only
   programs). Runs from one script. Everything after this is checked
   with it. *Built 2026-11-06*: `tests/lean/scripts/suite.sh`, the
   programs, how to run it and the first results (six programs that fail
   on Zune and not on MUI 3.8, eleven more kinds of difference) in
   [tests/lean/real-programs.md](../../tests/lean/real-programs.md).
2. **Demo set compared with MUI 3.8 automatically.** The 19 MUI 3.8
   demos and MUI-Demo's sub-windows, screenshotted on Zune and MUI 3.8
   with the same settings, compared pixel by pixel; a report of every
   difference. The goal is pixel-identical at default settings.
   *Built 2026-11-06* (branch `demopix`): `tests/lean/scripts/demopix.sh`
   with an HTML report; 8 of 27 windows pixel-identical on `lean`, 19
   after the fixes (25 can be compared at all). What is left (group
   distribution, Virtual's border scrollers, the Volumelist's spacing, scaled
   images and the Knob/Levelmeter look, the last two an artwork question) in
   [README.md](README.md#the-demo-set-pixel-by-pixel-against-mui-38-2026-11-06-phase-2).
3. **Smaller library** (target: code loaded at start <= 166 KB). More
   rarely used code into files loaded on demand (window snapshot and
   title gadgets, list editing and drag, string extras, ...), then the
   largest classes made smaller where they do work MUI 3.8 does not.
   Never at the cost of speed or features. *First round 2026-11-06*
   (branch `smaller`): title gadgets, rounded frames, custom frame
   drawing, window and list drag and drop, list editing and the pop-up
   menu strip into modules, one switch made a table: library file
   283,196 -> 268,572 bytes, library alone and first program 13.8 KB
   less, speed and stack as before, no module on a MUI 3 program's
   common path (checked by tracing every LoadSeg through the demos and
   the real-program suite: the rare vector images were tried as a module
   and taken back). 166 KB is not reached: the rest is the core classes'
   own code; what is still movable and what LTO would give in
   [README.md](README.md#a-smaller-library-2026-11-06-phase-3).
4. **Memory per program** (target: first program <= 460 KB, each
   further <= 210 KB): window backing store, list pools, notifications,
   Text contents. Same rule: never at the cost of speed or features.
   *First round 2026-11-06 (each further program reached, first program not yet)* (branch `memory`): measured first (memtally of every
   task, MUI 3.8 black-box: its second MUI-Demo holds 138.8 KB, Zune's
   144.6), then Area at MUI 3.8's 108 bytes, Group 96 -> 68, List 420 ->
   392, Image and Text smaller, a window's clip stack, buffer rastport and
   custom frames made when it opens, a Text's kept parse in a block of its
   size. MUI-Demo (FS-UAE, bytes): each further 217,240 -> 207,736 (target
   reached, below MUI 3.8's 210,696-218,824), first 488,016 -> 479,064 (not
   reached: the rest is the library's 43 KB of extra code, phase 3; Zune's
   first program on top of the library is 15 KB cheaper than MUI 3.8's).
   Speed and stack as before. Details in
   [README.md](README.md#memory-per-program-2026-11-06-phase-4).
   *Follow-up 2026-11-06* (branch `realmem`): why real programs showed
   10-110 KB more than on MUI 3.8 in the suite. Mostly the measurement:
   MUI 3.8 keeps its pool puddles after a program quits (AmFTP 112 KB, RO
   80, AmigaAMP-Prefs 56) and its second run reuses them; the suite now
   also records the first run from boot. What Zune really spent more: an
   event-node block for every shown object (now only for objects that take
   input, and 80 instead of 120 bytes) and an image spec per object for the
   plain pen backgrounds (now shared). Suite: 32 MUI 3.x programs 285 KB
   less together (RO -34 KB, AmigaAMP-Prefs -36, AmFTP -25), YAM 2.10 -223
   KB; MUI-Demo each further 204.5 KB, first 476.4 KB. First-run data now
   about MUI 3.8's (RO +7 KB, AmigaAMP-Prefs +22, AmFTP -54); the rest is
   code. Speed and stack as before (window opening 3.5% faster). Details in
   [README.md](README.md#real-programs-memory-against-mui-38-2026-11-06-phase-4-follow-up).
5. **Speed** (target: faster than MUI 3.8): start-up, window opening,
   redraw, list scrolling; measured on real 68000/68020 hardware, not
   only in the emulator, and never PiStorm figures as Amiga speeds. The
   real-hardware measurements are done by our partner on a spare A1200
   when one is available; until then emulator figures, labelled as such.
6. **MUI 3 behaviour exactly as MUI 3.8** and **MUI 4/5 programs run**:
   whatever the suite and the demo comparison find, fixed in order of
   how many programs it affects; MUI 4/5 features only as files loaded
   on demand, so MUI 3 programs pay nothing for them.
7. **Settings program as complete as MUI 3.8's**, including reading
   existing MUI 3.8 settings files (their item ids are known for the
   title gadgets; the rest to be measured).
8. **Each program gets the MUI it was written for** (decided with our
   partner 2026-11-06, done right after the phase 1 fixes): a MUI 3
   program sees and behaves as MUI 3.8, a MUI 4 program as MUI 4, a MUI 5
   program as MUI 5, with no setting to change. How: exec tells a
   library which version a program asked for when it opens it; the
   shared base reports 21 so every program gets in, and each opener gets
   its own small copy of the base reporting what it asked for (below 20:
   MUI 3.8). Zune's own behaviour follows the same choice per
   application. The "Behave as" setting stays and can override it:
   Automatic (the default), MUI 3.8, MUI 4 or MUI 5, for all programs,
   and per program in a list in Zune prefs. *Built 2026-11-06* (branch
   `identity`): one copy of the base per identity (342 bytes, shared),
   none per program; exec 40/47 pass the version in D2, not D0 as the
   RKRM's example says; classes get the identity of the program they are
   loaded for and are loaded again for another identity once unused.
   Limits: a class in use by one program serves a second program of
   another identity as the first's; most "MUI 4/5" programs ask for 19
   and so get MUI 3.8 under Automatic (only DizzyTorrent of the suite's
   five asks for 20): the per-program list gives them MUI 5. Details,
   measurements and the other limits in
   [README.md](README.md#each-program-gets-the-mui-it-asked-for-2026-11-06-phase-8).

## Later (wanted, not scheduled)

1. Hand-written 68000 assembly in the hottest drawing and layout loops.
2. The remaining AROS-derived code rewritten smaller, piece by piece.
3. MUI 5's extra classes and features as optional files.
4. Faster drawing on graphics cards (RTG).

## Bugs found from outside

1. **MUI_AslRequest returns at once without a requester** (found 2026-11-06 with PFS3Doctor on the A1200,
   Zune68 Beta 2, muimaster 21.227 of 2026-11-02): `MUI_AllocAslRequestTags(ASL_FileRequest,
   ASLFR_TitleText, ASLFR_InitialFile, ASLFR_DoSaveMode TRUE)` then `MUI_AslRequestTags(fr, ASLFR_Window, w)`
   showed nothing and saved nothing. Real MUI 3.8 (FS-UAE a1200-31-mui38) opens the requester with the same
   code. Suspect the MUI file panel path (`ZuneAllocAslRequest`/`ZuneAslRequest`, used when every tag is
   supported by the panel). PFS3Doctor now calls asl.library itself.
