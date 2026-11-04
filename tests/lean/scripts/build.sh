#!/usr/bin/env bash
# build.sh SRCTREE OUTDIR CPU MUI5 [make args]: build the core library twice from SRCTREE, as
# OUTDIR-sym (symbols, link map: for memtally reports) and OUTDIR (linked with -s, relocations kept;
# do not use m68k-amigaos-strip from bebbo's 6.5 toolchain, it drops them). The toolchain, sfdc,
# fd2sfd and a flexcat (or tools/lean/flexcat-sd.py named flexcat) must be on PATH. GCC 6.5 has
# no -std=gnu17: gnu11 is used, the code builds unchanged.
set -u
SRC=$1; OUT=$2; CPU=$3; M5=$4; shift 4
CF='-std=gnu11 -Os -g -Wall -Wno-pointer-sign -falign-functions=4 -falign-loops=4 -falign-jumps=4'
cd "$SRC" || exit 1
VAR=""; grep -q '^MUI5_TEST' workbench/libs/muimaster/GNUmakefile && VAR="MUI5_TEST=$M5"
make -j4 core BUILD=$OUT-sym CPUFLAGS="$CPU -msoft-float" CFLAGS="$CF" $VAR "$@" > $OUT-sym.log 2>&1 || { echo "sym build failed"; tail -20 $OUT-sym.log; exit 1; }
make -j4 core BUILD=$OUT CPUFLAGS="$CPU -msoft-float" CFLAGS="$CF" LDFLAGS=-s $VAR "$@" > $OUT.log 2>&1 || { echo "build failed"; tail -20 $OUT.log; exit 1; }
ls -la $OUT/muimaster.library
