#!/usr/bin/env bash
# bench.sh NAME MACHINE PORT PERFBENCH OUT [LIBFILE]: cold-boot an instance (with LIBFILE
# installed, if given), run tests/native/perfbench screen-fixed there and copy its output to OUT.
# Pass FS-UAE options as EXTRA. Build perfbench with bebbo's GCC 6.5 (BUILD = a library build):
#   m68k-amigaos-gcc -m68000 -noixemul -Os -DMUI_OBSOLETE -I$BUILD/include
#     -Iworkbench/libs/muimaster/include -Wl,-u,___stkinit tests/native/perfbench.c -o perfbench
#     -lmui -lamiga
# (-DPERFBENCH_REPS=3 for a quick run: 15 repetitions take ~5 minutes on a 68EC020.)
set -u
. "$(dirname "$0")/emu.sh"   # EMU=amiberry|fsuae: which emulator boots the machine
NAME=$1; MACH=$2; PORT=$3; PB=$4; OUT=$5; LIB=${6:-}
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
boot() { amiga-test $MACH stop --as $NAME >/dev/null 2>&1; ( amiga-test $MACH start --as $NAME --port $PORT ${EXTRA:-} >/dev/null 2>&1 < /dev/null & ); for i in $(seq 1 40); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3; }
boot
if [ -n "$LIB" ]; then "$(dirname "$0")/putlib.sh" "$LIB"; boot; fi
wasabi put "$PB" RAM:perfbench >/dev/null; wasabi run "protect RAM:perfbench +e" >/dev/null
sleep 10
wasabi run "stack 70000
RAM:perfbench screen-fixed" > "$OUT" 2>&1
cat "$OUT"
