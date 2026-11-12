#!/usr/bin/env bash
# opbench.sh NAME PORT OUT [LIBDIR] [REPS]: cold-boot a1200-31-mui38 as instance NAME on PORT in
# cycle-exact FS-UAE (warp mode: the emulated time, which the EClock counts, is unchanged), install
# OpenMUI from LIBDIR (its muimaster.library and Zune/ class files) if given, else MUI 3.8 stays,
# run tests/native/opbench there (REPS repetitions, default 9) and copy its output to OUT.
# OPBENCH = the opbench binary (default $ZWORK/opbench; build line in opbench.c). FRESH=0 keeps the
# instance's copy (default: reset), WARP=0 runs at the real speed, KEEP=1 leaves it running.
# Compare outputs with opbench-compare.py.
set -u
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
NAME=$1; PORT=$2; OUT=$3; LIB=${4:-}; REPS=${5:-9}
S=$(cd "$(dirname "$0")" && pwd)
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
CFG=(--config 'uae_cpu_cycle_exact = true' --config 'uae_blitter_cycle_exact = true'
     --config 'uae_cycle_exact = true' --config "warp_mode = ${WARP:-1}")
boot() {
  amiga-test a1200-31-mui38 stop --as $NAME >/dev/null 2>&1
  ( amiga-test a1200-31-mui38 start --as $NAME --port $PORT "${CFG[@]}" >/dev/null 2>&1 < /dev/null & )
  for i in $(seq 1 60); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3
}
[ "${FRESH:-1}" = 1 ] && amiga-test a1200-31-mui38 reset --as $NAME >/dev/null 2>&1
boot
if [ -n "$LIB" ]; then "$S/putlib.sh" "$LIB/muimaster.library" >/dev/null; boot; fi
wasabi put "${OPBENCH:-$ZWORK/opbench}" RAM:opbench >/dev/null; wasabi run "protect RAM:opbench +e" >/dev/null
sleep 5
wasabi run "stack 70000
version muimaster.library
RAM:opbench $REPS" > "$OUT" 2>&1
[ "${KEEP:-0}" = 1 ] || amiga-test a1200-31-mui38 stop --as $NAME >/dev/null 2>&1
cat "$OUT"
