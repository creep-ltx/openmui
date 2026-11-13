#!/usr/bin/env bash
# opbench.sh NAME PORT OUT [LIBDIR] [REPS]: cold-boot a1200-31-mui38 as instance NAME on PORT in
# cycle-exact FS-UAE (warp mode: the emulated time, which the EClock counts, is unchanged), install
# OpenMUI from LIBDIR (its muimaster.library and OpenMUI/ class files) if given, else MUI 3.8 stays,
# run tests/native/opbench there (REPS repetitions, default 9) and copy its output to OUT.
# OPBENCH = the opbench binary (default $ZWORK/opbench; build line in opbench.c). FRESH=0 keeps the
# instance's copy (default: reset), WARP=0 runs at the real speed, KEEP=1 leaves it running.
# MACH = the test machine (default a1200-31-mui38; a1200-32-mui5 is OS 3.2.1 + MUI 5: with LIBDIR
# its MUI:Libs is moved aside first, so OpenMUI is the only muimaster.library and no MUI 5 class
# file is found). ARGS = opbench's options before REPS (v21 = open version 21: OpenMUI's MUI 5
# identity; count / time = the OS calls per operation, tests/native/syscount.h).
# CFGX = one more FS-UAE setting (e.g. 'fast_memory = 4096': the a600-31 machine has 1 MB of chip
# RAM only, too little for opbench's application under MUI 3.8 or OpenMUI).
# Compare outputs with opbench-compare.py.
set -u
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
NAME=$1; PORT=$2; OUT=$3; LIB=${4:-}; REPS=${5:-9}
S=$(cd "$(dirname "$0")" && pwd)
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
CFG=(--config 'uae_cpu_cycle_exact = true' --config 'uae_blitter_cycle_exact = true'
     --config 'uae_cycle_exact = true' --config "warp_mode = ${WARP:-1}")
[ -n "${CFGX:-}" ] && CFG+=(--config "$CFGX")
MACH=${MACH:-a1200-31-mui38}
boot() {
  amiga-test $MACH stop --as $NAME >/dev/null 2>&1
  ( amiga-test $MACH start --as $NAME --port $PORT "${CFG[@]}" >/dev/null 2>&1 < /dev/null & )
  for i in $(seq 1 60); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3
}
[ "${FRESH:-1}" = 1 ] && amiga-test $MACH reset --as $NAME >/dev/null 2>&1
if [ -n "$LIB" ] && [ "$MACH" = a1200-32-mui5 ]; then
  D=$(amiga-test $MACH dir --as $NAME)
  [ -d "$D/MUI/Libs" ] && mv "$D/MUI/Libs" "$D/MUI/Libs.off" && mv -f "$D/MUI/Libs.uaem" "$D/MUI/Libs.off.uaem" 2>/dev/null
fi
boot
if [ -n "$LIB" ]; then "$S/putlib.sh" "$LIB/muimaster.library" >/dev/null; boot; fi
wasabi put "${OPBENCH:-$ZWORK/opbench}" RAM:opbench >/dev/null; wasabi run "protect RAM:opbench +e" >/dev/null
sleep 5
wasabi run "stack 70000
version muimaster.library
RAM:opbench ${ARGS:-} $REPS" > "$OUT" 2>&1
[ "${KEEP:-0}" = 1 ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1
cat "$OUT"
