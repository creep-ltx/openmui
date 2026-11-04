# Lean edition: GCC 16.2 against bebbo's GCC 6.5 (2026-11-04)

Stefan's CI and release builds use the Docker image
`stefanreinauer/amiga-gcc:gcc-v16.2` (AmigaPorts' m68k-amigaos-gcc 16.2,
NDK 3.2). This PC has no Docker, so the same toolchain was built from source
and installed next to the old one. The lean edition (step 13, `08e1f52e`) was
then built with both compilers and measured the same way as in
[measurements.md](measurements.md).

## The 16.2 toolchain: where it is and how it was built

- **Installed in `~/opt/amiga-gcc16`** (372 MB). bebbo's GCC 6.5 in
  `~/opt/amiga` is untouched; every other project keeps using it.
- **Source:** Stefan's own build script, `build.sh` from
  [reinauer/container-amiga-gcc](https://github.com/reinauer/container-amiga-gcc)
  (head `3f540ce682`; the same file is inside the image). It clones
  [AmigaPorts/m68k-amigaos-gcc](https://github.com/AmigaPorts/m68k-amigaos-gcc)
  (`8a428fa`), switches GCC to branch `amiga16.2`, and builds everything the
  image has: GCC 16.2.1b ("AmigaDev 16.2-rc23"), binutils 2.46 (`amiga-2.46`),
  libnix, clib2, newlib, NDK 3.2, sfdc, fd2sfd, fd2pragma, vasm, vlink, vbcc,
  lha, ira, gencrc, zlib/libpng/freetype, FlexCat (AmigaPorts fork, commit
  `91783ef2`) and amitools/vamos with machine68k (reinauer/amitools,
  `Drop_AccessStruct`) in `libexec/amitools`.
- **Command** (`~/opt/amiga-gcc16-build/run-build.sh`):

  ```
  build.sh --skip-apt --version 16.2:amiga16.2 --prefix ~/opt/amiga-gcc16 \
    --workdir ~/opt/amiga-gcc16-build/work --jobs 6
  ```

  11.5 minutes at `-j6`, no errors, peak load 9.6, never less than 20 GB of
  memory free.
- **Traps on this PC (Arch-based, not Ubuntu):**
  - `--skip-apt`, or the script tries `sudo apt-get`.
  - It requires `srec_cat` (the srecord package), which is not installed
    here. A stub that only logs and fails was put on `PATH`; it was never
    called, so the m68k toolchain does not need it.
  - Keep `~/opt/amiga/bin` off `PATH` while building: the zlib step calls
    `m68k-amigaos-ranlib` by name.
  - Do not pass `--link-default` (it wants to point `/opt/amiga` at the new
    tree, with sudo).
  - In an interactive zsh, `make` is an alias for `make -j16`; scripts are
    not affected.
- **Use it** by putting it first on `PATH`:

  ```
  PATH=~/opt/amiga-gcc16/bin:$PATH make core           # or check, release
  PATH=~/opt/amiga-gcc16/bin:$PATH tests/lean/scripts/build.sh ...
  ```

  `make check release` passes with it: FlexCat makes the catalogs and
  `vamos` runs the checks (`PYTHON=~/opt/amiga-gcc16/libexec/amitools/bin/python`,
  as in CI). FlexCat prints ~160 iconv warnings for some third-party `.po`
  files (BetterString's Polish catalog, for example). They come from those
  files, not from the toolchain.

## Does it match Stefan's toolchain?

- **Byte for byte, almost.** The image's own compiler (from the image's
  layer, run on this PC) and ours built the lean library and 33 of its 35
  class files identically. The one difference is `memset` (156 instead of
  168 bytes): libnix's "memset-movem" change was merged a few hours after
  the image was built. Two class files differ only in where the libgcc
  float helpers sit.
- **Beta 2.** Tag `v2026.11.02` built with our 16.2 and stripped as the
  release does (`m68k-amigaos-strip --strip-all`): 419,648 bytes; Stefan's
  Aminet Beta 2 library (68000 build) is 420,648 bytes. It was built two days earlier with an older
  image (rc21 era), and the gap is 0.24%. Upstream main (`3b9299d5`) gives
  419,708 bytes.

## bebbo's `-fbbb` is in 16.2 too

AmigaPorts' 16.2 has bebbo's back-end passes (`-fbbb`, default `+` =
`abcefilmnprsz0`, including the "i" auto-increment pass that broke
Numeric under 6.5). So the probe from `cd4f882e` does give 16.2 the same
`-fbbb=abcefhlmnprsz0`. That is harmless: built with and without the "i"
pass, the 16.2 library is byte-identical and every object's disassembly
is the same. measurements.md said GCC 13/16 have no `-fbbb`, which was
wrong.

## Known 6.5 miscompiles, run under vamos

The six miscompiles recorded in the knowledge base were collected into one
test program: `r[i] = l[i] - r[i]` in a loop, four byte compares in one `&&`
chain, a 12-byte struct passed and returned by value, a three-term loop
condition, an `unsigned char` field compared in a loop, and an inlined
pointer search. Each was built at -O1/-O2/-Os, for 68000/68020, with and
without `-fbbb=-`, and run under vamos:

| | 12 builds each |
|---|---|
| GCC 6.5 | the subtract loop is wrong at -O2 and -Os for 68000 (with or without `-fbbb=-`); at -O2 with the default `-fbbb` the program **crashes** (bad memory read), on both 68000 and 68020 |
| GCC 16.2 | **all correct** in all 12 builds |

## The comparison (lean step 13, FS-UAE)

Same flags for both: `-std=gnu11 -Os -g -falign-functions=4
-falign-loops=4 -falign-jumps=4 -msoft-float`, linked with `-s`. MUI-Demo,
cold, OS 3.1, A1200 68EC020, 2 MB chip + 4 MB fast. The 6.5 and 16.2 runs
of each pair ran at the same time on the PC, so both had the same load.

| | 6.5, 68000 | **16.2, 68000** | 6.5, 68020 | **16.2, 68020** |
|---|---:|---:|---:|---:|
| library file | 314,512 | **307,120** (-2.4%) | 309,656 | **303,776** (-1.9%) |
| code + data (text) | 293,860 | 287,744 | 290,296 | 285,352 |
| 35 class files | 163,328 | 162,680 | 160,720 | 160,916 |
| library only | 323.2 KB | **317.1 KB** | 319.7 KB | **314.7 KB** |
| first MUI-Demo | 553.4 KB | **547.0 KB** | 549.2 KB | **544.2 KB** |
| each further | 243.4 KB | 243.4 KB | 243.4 KB | 243.4 KB |
| first window (measure.sh, 2 runs) | 1.64 / 1.72 s | 1.56 / 1.70 s | 1.70 / 1.56 s | 1.68 / 1.68 s |
| window times 1st/2nd/3rd (wintimes.sh) | 1.72 / 1.43 / 1.41 s | 1.72 / 1.39 / 1.41 s | 1.68 / 1.41 / 1.47 s | 1.68 / 1.41 / 1.41 s |
| fullcheck (19 demos + 8 sub-windows) | all open, -48..+128 B | all open, -48..+128 B | all open, -48..+128 B | all open, -48..+128 B |
| real programs | all as before | same | same | same |

The 6.5 68000 row matches the step 13 row in measurements.md exactly
(323.2 / 553.4 / 243.4 KB), so the method reproduces.

**perfbench** (`bench.sh`, 15 repetitions, median, ms per batch, the same
perfbench executable for all):

| batch | 6.5, 68000 | 16.2, 68000 | | 6.5, 68020 | 16.2, 68020 | |
|---|---:|---:|---:|---:|---:|---:|
| create/dispose tree | 110.4 | 106.7 | -3.3% | 107.7 | 106.9 | -0.8% |
| open/close window | 399.8 | 399.8 | 0 | 399.7 | 399.5 | 0 |
| redraw x10 | 1237.5 | 1226.8 | -0.9% | 1232.0 | 1217.1 | -1.2% |
| relayout x5 | 851.2 | 845.8 | -0.6% | 841.1 | 829.6 | -1.4% |
| insert 200 | 940.0 | 931.5 | -0.9% | 938.5 | 929.0 | -1.0% |
| list active x50 | 1130.2 | 1129.3 | 0 | 1122.0 | 1123.5 | +0.1% |
| notify x100 | 600.6 | 601.5 | +0.2% | 585.1 | 584.8 | 0 |
| text set x100 | 240.1 | 240.8 | +0.3% | 239.2 | 239.8 | +0.3% |

**Real programs** (`real.sh`: OS 3.1 + 4 MB fast + 68882), all four builds:
NList-Demo works (about 2 KB more in use after the second run, as before),
NListtree-Demo opens no window (as on main), TextEditor-Demo works with 0
lost, TheBar demo1-10 all open with 0 lost (demo4 -240 B), YAM 2.5 opens
its charset warning, AmiSSL requester and error window. Every screenshot
of the 6.5 and 16.2 runs is pixel-identical, except the Layout demo, which
places its buttons at random.

**Code size per object** (68000): of 220 objects, 90 got smaller and 67
larger. The biggest wins are String -1,268 bytes, Window -1,044, List
-772, Application -544, Configdata -520 and Area -456. The biggest losses
are textengine +376, frame +344, Levelmeter +292 and Register +200.
Total -7.7 KB.

**Build time:** the same with both (8.5-8.7 s for the two lean builds at
`-j6`).

## Recommendation

**Build Zune68 with GCC 16.2.** It is what Stefan's releases use, it is
6 KB smaller in memory (68000) and 1-3% faster in the batches that
create objects and lay them out, it is never slower, and it gets right the
code that 6.5 is known to get wrong. The lean edition builds with it unchanged:
no Makefile change is needed (the `-fbbb` probe applies but changes no
code). Keep 6.5 working: the build still accepts it, and all the
measurements before this file were made with it.

For the other Amiga projects on this PC, 16.2 is worth trying one project
at a time, starting with the ones that hit the 6.5 miscompiles (CFile,
Markdown, CUtils, csh). It needs a check per project: binutils 2.46,
libnix and the NDK 3.2 headers differ from the 6.5 tree, and the knowledge
base records one GCC 16 m68k miscompile (`-flate-combine-instructions` at
-O2, m68k-elf), not seen here at `-Os`.

(*Verified* 2026-11-04: built, run in FS-UAE on own amiga-test instances,
measured. FS-UAE figures, not real hardware.)
