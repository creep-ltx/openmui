#!/usr/bin/env bash
# smallcheck.sh NAME PORT FASTKB: fullcheck's demos on the a600-31 test machine (a stock A600:
# 68000, OS 3.1, 1 MB chip RAM) with FASTKB of fast RAM (0, 1024 or 2048), the library LIB
# installed (gate 2, "works on a stock 68000, OS 3.1, 2 MB"). Boots the instance, installs the
# library and its class files, reboots, then runs fullcheck.sh (MUI-Demo with its eight
# sub-windows, the other 18 MUI 3.8 demos, each opened, shot and closed, the memory difference
# printed) and reports resets or freezes. Output: fullcheck's lines into OUT (default
# $ZWORK/smallcheck/NAME.txt), screenshots $ZWORK/suite/NAME-*.png.
#   LIB (default /tmp/stab1/build/muimaster.library), KEEP=1 leaves the instance running
set -u
. "$(dirname "$0")/emu.sh"
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
S=$(cd "$(dirname "$0")" && pwd)
NAME=$1; PORT=$2; FAST=${3:-0}
LIB=${LIB:-/tmp/stab1/build/muimaster.library}
MACH=a600-31
OUT=${OUT:-$ZWORK/smallcheck/$NAME.txt}
mkdir -p "$(dirname "$OUT")" "$ZWORK/logs"
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
WASABI_BIN=$(type -P wasabi)
wasabi() { timeout 60 "$WASABI_BIN" "$@"; }
log() { echo "$(date +%T) $NAME: $*" | tee -a "$OUT" >&2; }
boot() {
  amiga-test $MACH stop --as $NAME >/dev/null 2>&1
  local cfg=()
  [ "$FAST" -gt 0 ] && cfg=(--config fast_memory=$FAST)
  ( amiga-test $MACH start --as $NAME --port $PORT "${cfg[@]}" > "$ZWORK/logs/$NAME.log" 2>&1 < /dev/null & )
  local i
  for i in $(seq 1 90); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done
  for i in $(seq 1 40); do wasabi windows 2>/dev/null | grep -q '^ *window "Workbench"' && break; sleep 1; done
  sleep 3
}
: > "$OUT"
boot; "$S/putlib.sh" "$LIB" >/dev/null; boot
log "a600-31 with $FAST KB fast: $(wasabi run avail 2>/dev/null | awk '/^total/{print "in use", $3, "free", $2}')"
"$S/fullcheck.sh" "$NAME" "$PORT" 2>&1 | tee -a "$OUT"
if wasabi ping >/dev/null 2>&1; then log "machine alive at the end; alerts: $(wasabi info 2>/dev/null | grep -i -c -e 'guru medit' -e 'alert [0-9a-f]')"
else log "the machine does not answer at the end (reset or frozen)"; fi
[ -n "${KEEP:-}" ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1
