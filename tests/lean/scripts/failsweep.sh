#!/usr/bin/env bash
# failsweep.sh NAME PORT PROG N0 N1 [STEP]: fault injection over a program's start-up (gate 2,
# "out of memory never crashes"). Boots an a1200-31 instance NAME on PORT with the library LIB
# installed, copies tests/lean/memtally's failn (FAILN) and PROG (a file on the PC, or a name in
# $ZWORK/demos) to RAM:, then for N = N0..N1 (step STEP, default 1) runs PROG under
# "failn run PROG N": the Nth resource call made from library code fails (NULL). For each N:
# whether a window opened, the windows are closed with their close gadgets (requesters answered
# by their buttons), whether the program ended, whether the machine reset or hung (a hung program
# is killed), the memory in use before the start minus after the end (negative: memory kept),
# and failn's report (the function that failed and the callers). N1 = all: up to the count of a
# run that fails nothing (printed first). One TSV line per N into OUT (default
# $ZWORK/failsweep/NAME.tsv): N, status, windows, lost, report.
#   LIB    muimaster.library to install (default /tmp/stab1/build/muimaster.library); LIB= (empty)
#          installs nothing: with MACH=a1200-31-mui38 the sweep runs on real MUI 3.8 (failn
#          then reports no library ranges; MUI 3.8 is a black box)
#   MACH   the test machine (default a1200-31)
#   FAILN  the failn binary (default $ZWORK/failn, built here from tests/lean/memtally if missing)
#   MASK   hex mask of the hooked functions (failn.c's table; default all)
#   WAIT   seconds to wait for a window (default 6)
#   REXXPORT the program's ARexx port, asked to QUIT when its window ignores the close gadget
#          (default MUIDEMO.1)
#   FAST   fast RAM in KB (default 8192)
#   FLUSH=1 "avail flush" before each memory figure: the library and class files a quit
#          program leaves loaded are flushed too, so "lost" is only what cannot be given back
#   KEEP=1 leaves the instance running
# Status: ok (window, closed, ended), nowindow (ended without a window: a clean refusal),
# hung (did not end: killed), reset (the machine rebooted), frozen (wasabid gone), left
# (ended but a window of another task stayed, e.g. a requester).
set -u
. "$(dirname "$0")/emu.sh"
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
S=$(cd "$(dirname "$0")" && pwd)
NAME=$1; PORT=$2; PROG=$3; N0=$4; N1=$5; STEP=${6:-1}
LIB=${LIB:-/tmp/stab1/build/muimaster.library}
FAILN=${FAILN:-$ZWORK/failn}
MASK=${MASK:-}
WAIT=${WAIT:-6}
FAST=${FAST:-8192}
MACH=${MACH:-a1200-31}
OUT=${OUT:-$ZWORK/failsweep/$NAME.tsv}
mkdir -p "$(dirname "$OUT")" "$ZWORK/logs"
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
WASABI_BIN=$(type -P wasabi)
wasabi() { timeout 40 "$WASABI_BIN" "$@"; }
[ -f "$PROG" ] || PROG=$ZWORK/demos/$PROG
[ -f "$PROG" ] || { echo "failsweep: no program $PROG" >&2; exit 2; }
if [ ! -f "$FAILN" ]; then
  ( cd "$S/../memtally" && m68k-amigaos-gcc -Os -m68000 -noixemul -s -o "$FAILN" failn.c failn.s callseg.s ) || exit 1
fi
log() { echo "$(date +%T) $NAME: $*" >&2; }

boot() {
  amiga-test $MACH stop --as $NAME >/dev/null 2>&1
  ( amiga-test $MACH start --as $NAME --port $PORT --config fast_memory=$FAST --config fpu=68882 > "$ZWORK/logs/$NAME.log" 2>&1 < /dev/null & )
  local i
  for i in $(seq 1 60); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done
  for i in $(seq 1 30); do wasabi windows 2>/dev/null | grep -q '^ *window "Workbench"' && break; sleep 1; done
  sleep 2
}
putfiles() {
  wasabi put "$FAILN" RAM:failn >/dev/null; wasabi run "protect RAM:failn +e" >/dev/null
  wasabi put "$PROG" RAM:Prog >/dev/null; wasabi run "protect RAM:Prog +e" >/dev/null
}
avail() { [ -n "${FLUSH:-}" ] && { wasabi run "avail flush" >/dev/null; sleep 1; wasabi run "avail flush" >/dev/null; }; sleep 1; wasabi run avail 2>/dev/null | awk '/^total/{print $3}'; }
wins() { wasabi windows 2>/dev/null | grep '^ *window' | grep -v '"Workbench"'; }
running() { wasabi ps 2>/dev/null | grep -q 'RAM:Prog'; }   # failn names the process after the program
alive() { wasabi ping >/dev/null 2>&1 || { sleep 2; wasabi ping >/dev/null 2>&1; }; }
close_windows() { # close gadgets, then requester buttons; 0 when no window is left
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

boot; [ -n "$LIB" ] && { "$S/putlib.sh" "$LIB" >/dev/null; boot; }; putfiles
if [ "$N1" = all ] || [ "$N0" = count ]; then
  wasabi run "stack 40000
run >NIL: RAM:failn run RAM:Prog 0 >RAM:fn.txt" >/dev/null
  for i in $(seq 1 40); do [ -n "$(wins)" ] && break; sleep 0.3; done
  sleep 2; close_windows
  for i in $(seq 1 20); do running || break; sleep 1; done
  running && wasabi kill RAM:Prog >/dev/null 2>&1
  sleep 1
  COUNT=$(wasabi run "type RAM:fn.txt" 2>/dev/null | tr -d '\r' | grep -o 'counted [0-9]*' | awk '{print $2}')
  log "fails nothing: $(wasabi run "type RAM:fn.txt" 2>/dev/null | tr -d '\r' | grep counted)"
  [ "$N0" = count ] && { [ -n "${KEEP:-}" ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1; exit 0; }
  N1=${COUNT:-0}
fi
log "sweep $N0..$N1 step $STEP of $(basename "$PROG") -> $OUT"
N=$N0
while [ "$N" -le "$N1" ]; do
  status=ok; wl=; rep=
  A0=$(avail)
  wasabi run "stack 40000
run >NIL: RAM:failn run RAM:Prog $N ${MASK:+$MASK} >RAM:fn.txt" >/dev/null 2>&1
  t0=$SECONDS; haswin=0
  while [ $((SECONDS - t0)) -lt "$WAIT" ]; do
    if [ -n "$(wins)" ]; then haswin=1; break; fi
    running || break
    sleep 0.4
  done
  if ! alive; then
    status=reset
  else
    if [ $haswin = 1 ]; then
      sleep 1; wl=$(wins | sed 's/^ *window //; s/  task.*//' | tr '\n' ';')
      close_windows || status=left
    else status=nowindow; fi
    for i in $(seq 1 10); do running || break; sleep 1; done
    if running; then status="$status,hung"; wasabi arexx --timeout 5 "${REXXPORT:-MUIDEMO.1}" QUIT >/dev/null 2>&1; sleep 2; running && { wasabi kill RAM:Prog >/dev/null 2>&1; sleep 2; }; running && { wasabi kill --force RAM:Prog >/dev/null 2>&1; sleep 2; }; fi
    [ -n "$(wins)" ] && { close_windows; status="$status,left"; }
  fi
  if ! alive; then
    for i in $(seq 1 30); do sleep 2; wasabi ping >/dev/null 2>&1 && break; done
    if wasabi ping >/dev/null 2>&1; then
      status=reset; sleep 3; putfiles   # RAM: is empty after the reset; the record is in chip RAM
      rep=$(wasabi run "RAM:failn read" 2>/dev/null | tr -d '\r' | grep 'failn: ')
      log "N=$N: the machine reset ($rep)"
      A1=$A0
    else
      status=frozen; log "N=$N: frozen, rebooting"; boot; putfiles; A1=$A0
    fi
  else
    rep=$(wasabi run "type RAM:fn.txt" 2>/dev/null | tr -d '\r' | grep 'failn:' | tr '\n' ' ')
    A1=$(avail)
  fi
  lost=$(( ${A0:-0} - ${A1:-0} ))
  printf '%s\t%s\t%s\t%s\t%s\n' "$N" "$status" "$wl" "$lost" "$rep" | tee -a "$OUT"
  N=$((N + STEP))
done
[ -n "${KEEP:-}" ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1
