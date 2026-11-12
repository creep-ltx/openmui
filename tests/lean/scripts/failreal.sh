#!/usr/bin/env bash
# failreal.sh NAME PORT ID N0 N1 [STEP]: failsweep.sh for a program of the real-program suite
# (tests/lean/real/programs.sh): the program and its classes installed as suite.sh does (only
# that program), then for N = N0..N1 (step STEP) the program runs under "failn run PROG N" from
# its own folder: the Nth resource call made from library code fails. Each N: the suite's
# pre hook, the first window (up to the table's wait), the dismiss hook, the suite's quit (its
# quit hook or menu, requesters, close gadgets), a kill by address if it still runs. N1 = all:
# up to the count of a run that fails nothing (N0 = count only prints it).
# One TSV line per N into OUT (default $ZWORK/failreal/NAME.tsv): N, status, windows, lost
# (in use before minus after; negative: kept), failn's report.
#   LIB    muimaster.library (default /tmp/stab2/build/muimaster.library; its Zune/ and Libs/MUI
#          class files next to it, as suite.sh)
#   FLUSH=1 "avail flush" twice before each memory figure (default 1: what a failed start loads
#          on demand is flushed, so "lost" is what cannot be given back)
#   WAIT   cap on the window wait in seconds (default: the table's)
#   SAMPLES=K with N1 = all: about K N's spread evenly over the count (a sampled sweep: a real
#          program makes 2,000-8,000 counted calls at 15 s each)
#   MASK   hex mask of the hooked functions (failn.c's table), as failsweep.sh; dfff leaves out
#          NewObjectA: intuition's NewObjectA with a class pointer cannot fail before the
#          class's OM_NEW runs (the instance is allocated in rootclass's OM_NEW, which the
#          AllocMem hook fails), so failing the call itself is a failure that never happens
#   REUSE=1 boot the instance's copy as it is (no fresh install)
#   KEEP=1 leave the instance running
# Status: ok, nowindow (ended without a window: a clean refusal), hung (did not end: killed),
# left (a window stayed), reset (the machine rebooted: failn's record read back), frozen.
set -u
. "$(dirname "$0")/emu.sh"
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
REAL=${REAL:-$HOME/.cache/zune68k-real}
S=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$S/../../.." && pwd)
[ -d "$HOME/opt/amiga-gcc16/bin" ] && PATH=$HOME/opt/amiga-gcc16/bin:$PATH
NAME=$1; PORT=$2; WANT=$3; N0=$4; N1=$5; STEP=${6:-1}
TARGET=zune; MACH=a1200-31
LIB=${LIB:-/tmp/stab2/build/muimaster.library}
FAILN=${FAILN:-$ZWORK/failn}
FLUSH=${FLUSH:-1}
OUT=${OUT:-$ZWORK/failreal/$NAME.tsv}
LOG=$ZWORK/failreal/$NAME.log
mkdir -p "$(dirname "$OUT")" "$ZWORK/logs" "$ZWORK/failreal"
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
log() { echo "$(date +%T) $NAME: $*" | tee -a "$LOG" >&2; }
. "$TOP/tests/lean/real/programs.sh"
. "$S/suite-lib.sh"
if [ ! -f "$FAILN" ]; then
  ( cd "$S/../memtally" && m68k-amigaos-gcc -Os -m68000 -noixemul -s -o "$FAILN" failn.c failn.s callseg.s ) || exit 1
fi
ENTRY=
for e in "${PROGS[@]}"; do parse_prog "$e"; [ "$P_ID" = "$WANT" ] && { ENTRY=$e; break; }; done
[ -n "$ENTRY" ] || { echo "failreal: no program $WANT in programs.sh" >&2; exit 2; }
PROGS=("$ENTRY"); parse_prog "$ENTRY"
WAITS=${WAIT:-$P_WAIT}; [ "$WAITS" -gt "$P_WAIT" ] && WAITS=$P_WAIT

amiga-test $MACH stop --as $NAME >/dev/null 2>&1
if [ "${REUSE:-0}" != 1 ]; then
  amiga-test $MACH reset --as $NAME >/dev/null
  host_install "$(amiga-test $MACH dir --as $NAME)"
fi
# RAM:failn doubles as the reset marker: Amiberry reboots in 2 s, so a reset can pass between two
# pings; a missing RAM:failn after a run means the machine rebooted
# TRAPCAP=FILE (tests/lean/trapcap built for 68020): the trap recorder runs all along; after a
# reset its record (the CPU exception: registers, code, stack) goes to failreal/NAME-N.trap
putfailn() { wasabi put "$FAILN" RAM:failn >/dev/null; wasabi run "protect RAM:failn +e
makedir ENV:MUI" >/dev/null 2>&1
  if [ -n "${TRAPCAP:-}" ]; then
    wasabi put "$TRAPCAP" RAM:trapcap >/dev/null; wasabi run "protect RAM:trapcap +e" >/dev/null 2>&1
    [ -n "${1:-}" ] && wasabi run "RAM:trapcap read" > "$ZWORK/failreal/$NAME-$1.trap" 2>/dev/null
    wasabi run "RAM:trapcap read clear" >/dev/null 2>&1
    wasabi run "run >NIL: RAM:trapcap" >/dev/null 2>&1; sleep 1
  fi; }
rebooted() { ! wasabi run "list RAM:failn QUICK" >/dev/null 2>&1; }   # its exit code: the "not found" message names the file too
boot || { log "instance did not come up"; exit 1; }
putfailn
RD="$(sysvol)Real"; dir="$RD/$P_ID"; exe=${P_EXE%% *}
if [ -n "${RAMRUN[$P_ID]:-}" ]; then wasabi run "copy \"$dir\" RAM:$P_ID ALL QUIET" >/dev/null 2>&1; dir=RAM:$P_ID; fi
P_TASK="$dir/$exe"; P_TITLE=${TITLE[$P_ID]:-}
memfree() { if [ "$FLUSH" = 1 ]; then wasabi run "avail flush" >/dev/null 2>&1; sleep 1; wasabi run "avail flush" >/dev/null 2>&1; fi; avail; }
pid() { wasabi ps 2>/dev/null | grep -F "${P_TASK:0:30}" | awk '{print $1}' | head -1; }
startn() { # N: the program under failn, from its folder
  wasabi run "cd \"$dir\"
stack 65536
run >NIL: <NIL: RAM:failn run \"$P_TASK\" $1 ${MASK:-} >RAM:fn.txt" >/dev/null 2>&1
}
# a program that does not end even when killed would stay for every later N (NListtree-Demo
# hung at N=5 and the rest of its sweep ran nothing): reboot then; the next figure starts fresh
stop_hard() { local a; a=$(pid); [ -n "$a" ] || return 0
  wasabi kill "$a" >/dev/null 2>&1; sleep 3; a=$(pid); [ -n "$a" ] && { wasabi kill --force "$a" >/dev/null 2>&1; sleep 3; }
  if [ -n "$(pid)" ]; then log "still running after kill: rebooting"; amiga-test $MACH stop --as $NAME >/dev/null 2>&1; boot; putfailn; warm; REBOOTED=1; fi; }

# a run that fails nothing: the count, and after every boot the first run's own loads (a program's
# first run from boot keeps what it loads for good: AmIRC 353 KB) out of the next figure
warm() { hook pre; startn 0
  wait_window "$WAITS" && { settle 5; hook dismiss; }
  quit_prog || stop_hard
  sleep 1; }
warm
if [ "$N1" = all ] || [ "$N0" = count ]; then
  COUNT=$(wasabi run "type RAM:fn.txt" 2>/dev/null | tr -d '\r' | grep -o 'counted [0-9]*' | awk '{print $2}')
  log "fails nothing: $(wasabi run "type RAM:fn.txt" 2>/dev/null | tr -d '\r' | grep counted)"
  [ "$N0" = count ] && { [ -n "${KEEP:-}" ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1; exit 0; }
  N1=${COUNT:-0}
  # SAMPLES=K: about K N's spread over the whole count (STEP from the count), starting at N0
  if [ -n "${SAMPLES:-}" ]; then STEP=$(( (N1 - N0) / SAMPLES )); [ "$STEP" -ge 1 ] || STEP=1; fi
fi
log "sweep $N0..$N1 step $STEP of $P_ID -> $OUT"
N=$N0
while [ "$N" -le "$N1" ]; do
  status=ok; wl=; rep=; REBOOTED=0
  A0=$(memfree)
  hook pre; startn "$N"
  if ! alive; then status=reset
  else
    if wait_window "$WAITS"; then
      settle 5; hook dismiss
      wl=$(wins | cut -f1,2 | tr '\t\n' '@;')
      quit_prog || { status=hung; stop_hard; }
    else
      status=nowindow
      if running; then status=nowindow,hung; stop_hard; fi
    fi
    alive && running && { status="$status,left"; stop_hard; }
    alive && [ -n "$(wins)" ] && { close_all; status="$status,left"; }
  fi
  if ! alive || rebooted; then
    if alive; then why=reset; else why=$(gone_why); fi
    if [ "$why" = reset ]; then
      status=reset; sleep 3; putfailn "$N"
      rep=$(wasabi run "RAM:failn read" 2>/dev/null | tr -d '\r' | grep 'failn: ' | tr '\n' ' ')
      log "N=$N: the machine reset ($rep)"; warm
    else
      status=frozen; log "N=$N: frozen, rebooting"; amiga-test $MACH stop --as $NAME >/dev/null 2>&1; boot; putfailn; warm
    fi
    A1=$A0
  else
    rep=$(wasabi run "type RAM:fn.txt" 2>/dev/null | tr -d '\r' | grep 'failn:' | tr '\n' ' ')
    if [ "$REBOOTED" = 1 ]; then A1=$A0; status="$status,rebooted"; else A1=$(memfree); fi
  fi
  lost=$(( ${A0:-0} - ${A1:-0} ))
  printf '%s\t%s\t%s\t%s\t%s\n' "$N" "$status" "$wl" "$lost" "$rep" | tee -a "$OUT"
  N=$((N + STEP))
done
[ -n "${KEEP:-}" ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1
