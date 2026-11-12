#!/usr/bin/env bash
# opprof.sh NAME PORT LIBDIR OPNAME COUNT SECONDS: profile one opbench operation of OpenMUI.
# Boots NAME (cycle-exact FS-UAE, no warp: pcsample's ticks are 20 ms of emulated time) with
# LIBDIR's library, runs "opbench prof OPNAME COUNT" detached; opbench does every operation before
# OPNAME three times, writes RAM:opbench.ready and waits (not running, so not sampled) until
# RAM:opbench.go exists. Then tests/lean/pcsample.c samples it for SECONDS and the profile is
# printed with every static function too (pcsample-full.py). LIBDIR must be a build with its link
# map (muimaster.library.map) and object files. OPBENCH, PCSAMPLE: the binaries (default in ZWORK;
# pcsample is built there if missing). TOP=N lines; the samples stay in ZWORK/prof-NAME.txt.
set -u
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
NAME=$1; PORT=$2; LIB=$3; OP=$4; COUNT=$5; SECS=$6
S=$(cd "$(dirname "$0")" && pwd)
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
PCS=${PCSAMPLE:-$ZWORK/pcsample}
[ -x "$PCS" ] || m68k-amigaos-gcc -Os -noixemul -o "$PCS" "$S/../pcsample.c" || exit 1
CFG=(--config 'uae_cpu_cycle_exact = true' --config 'uae_blitter_cycle_exact = true'
     --config 'uae_cycle_exact = true')
amiga-test a1200-31-mui38 reset --as $NAME >/dev/null 2>&1
boot() {
  amiga-test a1200-31-mui38 stop --as $NAME >/dev/null 2>&1
  ( amiga-test a1200-31-mui38 start --as $NAME --port $PORT "${CFG[@]}" >/dev/null 2>&1 < /dev/null & )
  for i in $(seq 1 60); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3
}
boot; "$S/putlib.sh" "$LIB/muimaster.library" >/dev/null; boot
wasabi put "${OPBENCH:-$ZWORK/opbench}" RAM:opbench >/dev/null; wasabi run "protect RAM:opbench +e" >/dev/null
wasabi put "$PCS" RAM:pcsample >/dev/null; wasabi run "protect RAM:pcsample +e" >/dev/null
wasabi run --detach "stack 70000
RAM:opbench prof \"$OP\" $COUNT" 2>/dev/null
# "list" names a missing file in its error text too: look for the protection bits
for i in $(seq 1 300); do
  wasabi run "list RAM:opbench.ready" 2>/dev/null | grep -q 'rwed' && break
  sleep 2
done
wasabi run --detach "RAM:pcsample opbench RAM:samples $SECS" 2>/dev/null
sleep 2
wasabi run "echo >RAM:opbench.go go" >/dev/null
for i in $(seq 1 300); do
  wasabi run "list RAM:samples" 2>/dev/null | grep -q 'rwed' && break
  sleep 3
done
sleep 3
wasabi get RAM:samples "$ZWORK/prof-$NAME.txt" >/dev/null 2>&1
[ "${KEEP:-0}" = 1 ] || amiga-test a1200-31-mui38 stop --as $NAME >/dev/null 2>&1
python3 "$S/pcsample-full.py" "$ZWORK/prof-$NAME.txt" "${OPBENCH:-$ZWORK/opbench}" "$LIB" ${TOP:-30}
