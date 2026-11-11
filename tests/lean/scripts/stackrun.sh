#!/usr/bin/env bash
# stackrun.sh NAME PORT LIB OUT: the deepest stack use of MUI-Demo (with its 8 sub-windows opened and
# closed), Layout, Virtual, Pages and Slidorama, each under tests/lean/stackmark, on a cold-booted
# a1200-31 instance with LIB (and its class files) installed; one line per demo into OUT.
# ZWORK (default ~/.cache/zune68-lean) holds demos/ (MUI 3.8's demo binaries) and logs/.
# The toolchain must be on PATH (stackmark is built into ZWORK). KEEP=1 leaves the instance running.
set -u
. "$(dirname "$0")/emu.sh"   # EMU=amiberry|fsuae: which emulator boots the machine
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
NAME=$1; PORT=$2; LIB=$3; OUT=$4; MACH=a1200-31
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
S=$(cd "$(dirname "$0")" && pwd)
SM=$ZWORK/stackmark
mkdir -p "$ZWORK/logs"
[ -f "$SM" ] || m68k-amigaos-gcc -Os -noixemul -m68000 -o "$SM" "$S/../stackmark.c" || exit 1
boot() { amiga-test $MACH stop --as $NAME >/dev/null 2>&1; ( amiga-test $MACH start --as $NAME --port $PORT --config fast_memory=4096 > $ZWORK/logs/$NAME.log 2>&1 < /dev/null & ); for i in $(seq 1 40); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3; }
boot; "$S/putlib.sh" "$LIB" >/dev/null; boot
wasabi put "$SM" RAM:stackmark >/dev/null
: > "$OUT"
one() { # DEMO [MUI-Demo sub-window buttons...]
  local d=$1 k w x y; shift
  wasabi put "$ZWORK/demos/$d" "RAM:$d" >/dev/null; wasabi run "protect RAM:$d +e" >/dev/null
  wasabi run "stack 40000
run >nil: RAM:$d" >/dev/null
  for i in $(seq 1 100); do [ "$(wasabi windows 2>/dev/null | grep -c "task RAM:$d")" -ge 1 ] && break; sleep 0.3; done
  sleep 3
  # stackmark needs the program waiting; try again while it is busy
  for k in 1 2 3 4 5 6; do
    wasabi run "run RAM:stackmark RAM:$d 1 >RAM:sm-$d.txt" >/dev/null
    sleep 2
    wasabi run "type RAM:sm-$d.txt" 2>/dev/null | grep -q 'not found' || break
    sleep 1
  done
  [ $# -gt 0 ] && TAG=stk-$NAME "$S/subwin.sh" "$@" >/dev/null 2>&1
  w=$(wasabi windows | grep "task RAM:$d" | tail -1)
  x=$(echo "$w" | sed 's/.* at \([0-9]*\),\([0-9]*\).*/\1/'); y=$(echo "$w" | sed 's/.* at \([0-9]*\),\([0-9]*\).*/\2/')
  wasabi mouse click $((x+6)) $((y+5)) >/dev/null
  for i in $(seq 1 20); do wasabi ps 2>/dev/null | grep -q "RAM:$d" || break; sleep 1; done
  wasabi kill "RAM:$d" >/dev/null 2>&1
  sleep 3
  echo "$d: $(wasabi run "type RAM:sm-$d.txt" 2>/dev/null | grep -e deepest -e 'not found' | tail -1)" | tee -a "$OUT"
}
one MUI-Demo Groups Frames Backfill Notify Listviews Cycle Images Strings
one Layout
one Virtual
one Pages
one Slidorama
[ -n "${KEEP:-}" ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1
