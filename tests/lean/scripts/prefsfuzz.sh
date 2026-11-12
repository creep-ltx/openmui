#!/usr/bin/env bash
# prefsfuzz.sh NAME PORT N0 N1 [PROG]: settings-file fuzz on the Amiga (gate 2, robust input).
# Boots an a1200-31 instance NAME on PORT with the library LIB installed and, for each seed
# N0..N1, writes tools/lean/prefsmutate.py's mutated settings file (a Zune global.prefs with
# every setting the library reads: fonts, pen/image/frame specs, key strings, ULONGs; one or
# more of them damaged) to ENV:Zune/global.prefs and ENV:Zune/<BASE>.prefs, starts PROG
# (default MUI-Demo from $ZWORK/demos) with it, closes its windows and checks that the program
# ended, that the machine neither reset nor froze, and how much memory stayed in use. Seed 0 is
# the undamaged file. One TSV line per seed into OUT (default $ZWORK/prefsfuzz/NAME.tsv): seed,
# status, windows, lost, the mutation's description. A seed can be run again with
# tools/lean/prefsmutate.py SEED FILE to look at the file.
#   LIB (default /tmp/stab1/build/muimaster.library), WAIT (6 s), FAST (8192), BASE (the
#   program's ARexx base for its own prefs file, default MUIDEMO), KEEP=1
set -u
. "$(dirname "$0")/emu.sh"
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
S=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$S/../../.." && pwd)
NAME=$1; PORT=$2; N0=$3; N1=$4; PROG=${5:-MUI-Demo}
LIB=${LIB:-/tmp/stab1/build/muimaster.library}
WAIT=${WAIT:-6}; FAST=${FAST:-8192}; BASE=${BASE:-MUIDEMO}
MACH=a1200-31
OUT=${OUT:-$ZWORK/prefsfuzz/$NAME.tsv}
mkdir -p "$(dirname "$OUT")" "$ZWORK/logs" "$ZWORK/prefsfuzz/files"
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
WASABI_BIN=$(type -P wasabi)
wasabi() { timeout 40 "$WASABI_BIN" "$@"; }
[ -f "$PROG" ] || PROG=$ZWORK/demos/$PROG
log() { echo "$(date +%T) $NAME: $*" >&2; }
boot() {
  amiga-test $MACH stop --as $NAME >/dev/null 2>&1
  ( amiga-test $MACH start --as $NAME --port $PORT --config fast_memory=$FAST --config fpu=68882 > "$ZWORK/logs/$NAME.log" 2>&1 < /dev/null & )
  local i
  for i in $(seq 1 60); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done
  for i in $(seq 1 30); do wasabi windows 2>/dev/null | grep -q '^ *window "Workbench"' && break; sleep 1; done
  sleep 2
}
putfiles() { wasabi put "$PROG" RAM:Prog >/dev/null; wasabi run "protect RAM:Prog +e" >/dev/null; wasabi run "makedir ENV:Zune" >/dev/null 2>&1; }
avail() { sleep 1; wasabi run avail 2>/dev/null | awk '/^total/{print $3}'; }
wins() { wasabi windows 2>/dev/null | grep '^ *window' | grep -v '"Workbench"'; }
running() { wasabi ps 2>/dev/null | grep -q 'RAM:Prog'; }
alive() { wasabi ping >/dev/null 2>&1 || { sleep 2; wasabi ping >/dev/null 2>&1; }; }
close_windows() {
  local k w x y t b
  for k in 1 2 3 4 5 6; do
    w=$(wins | head -1); [ -z "$w" ] && return 0
    x=$(echo "$w" | sed 's/.* at \([0-9]*\),\([0-9]*\).*/\1/'); y=$(echo "$w" | sed 's/.* at \([0-9]*\),\([0-9]*\).*/\2/')
    t=$(echo "$w" | sed 's/^ *window "\([^"]*\)".*/\1/')
    if [ $k -le 3 ]; then wasabi mouse click $((x+6)) $((y+5)) >/dev/null 2>&1
    else for b in Ok OK Okay Cancel Abort Continue Retry Quit Yes; do wasabi ui click "$b" --window "$t" >/dev/null 2>&1 && break; done
    fi
    sleep 1.5
  done
  [ -z "$(wins)" ]
}

boot; "$S/putlib.sh" "$LIB" >/dev/null; boot; putfiles
log "seeds $N0..$N1 with $(basename "$PROG") -> $OUT"
for N in $(seq "$N0" "$N1"); do
  F=$ZWORK/prefsfuzz/files/$NAME-$N.prefs
  desc=$(python3 -I "$TOP/tools/lean/prefsmutate.py" "$N" "$F" | sed 's/^[^:]*: //')
  wasabi put "$F" ENV:Zune/global.prefs --force >/dev/null 2>&1
  wasabi put "$F" "ENV:Zune/$BASE.prefs" --force >/dev/null 2>&1
  status=ok; wl=
  A0=$(avail)
  wasabi run "stack 40000
run >NIL: RAM:Prog" >/dev/null 2>&1
  t0=$SECONDS; haswin=0
  while [ $((SECONDS - t0)) -lt "$WAIT" ]; do
    if [ -n "$(wins)" ]; then haswin=1; break; fi
    running || break
    sleep 0.4
  done
  if ! alive; then status=reset
  else
    if [ $haswin = 1 ]; then sleep 1; wl=$(wins | sed 's/^ *window //; s/  task.*//' | tr '\n' ';'); close_windows || status=left
    else status=nowindow; fi
    for i in $(seq 1 10); do running || break; sleep 1; done
    if running; then status="$status,hung"; wasabi kill RAM:Prog >/dev/null 2>&1; sleep 2; running && { wasabi kill --force RAM:Prog >/dev/null 2>&1; sleep 2; }; fi
    [ -n "$(wins)" ] && { close_windows; status="$status,left"; }
  fi
  if ! alive; then
    for i in $(seq 1 30); do sleep 2; wasabi ping >/dev/null 2>&1 && break; done
    if wasabi ping >/dev/null 2>&1; then status=reset; sleep 3; putfiles; log "seed $N: the machine reset"
    else status=frozen; log "seed $N: frozen, rebooting"; boot; putfiles; fi
    A1=$A0
  else A1=$(avail); fi
  printf '%s\t%s\t%s\t%s\t%s\n' "$N" "$status" "$wl" "$(( ${A0:-0} - ${A1:-0} ))" "$desc" | tee -a "$OUT"
done
wasabi run "delete ENV:Zune/global.prefs ENV:Zune/$BASE.prefs QUIET" >/dev/null 2>&1
[ -n "${KEEP:-}" ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1
