#!/usr/bin/env bash
# soak.sh NAME PORT MINUTES: keeps MUI programs running and busy for a long time and samples the
# memory in use (gate 2, "hours of open/close loops flat"). Boots an a1200-31 instance NAME on
# PORT with the library LIB installed, starts MUI-Demo, Pages, Virtual and Slidorama (MUI 3.8's
# demos from $ZWORK/demos) and then, until MINUTES have passed, each cycle:
#   - opens each of MUI-Demo's eight sub-windows (Groups ... Strings), works in it (Listviews:
#     cursor keys and the scroller; Strings: typing and Return; Cycle: the cycle gadget and the
#     list) and closes it with its close gadget
#   - switches the Pages demo's pages with the cursor keys, scrolls the Virtual demo with its
#     scrollers, moves Slidorama's sliders with the keys
#   - iconifies and uniconifies MUI-Demo through its ARexx port (HIDE, SHOW)
#   - resizes MUI-Demo by its size gadget and back
# and writes one line per cycle to OUT (default $ZWORK/soak/NAME.tsv): cycle, seconds, memory
# in use (avail, bytes), the number of windows. A growing in-use figure with a constant
# number of windows is a leak: find it with memtally (TALLY=1 runs MUI-Demo under
# "memtally runall" and dumps its live blocks to OUT.mt at the end: tools/lean/mtlib.py).
# At the end every program is quit by its close gadget and the memory in use is compared with the
# figure before the first start (after avail flush on both sides): "lost".
#   LIB   muimaster.library (default /tmp/stab1/build/muimaster.library); FAST fast RAM KB (8192)
#   KEEP=1 leaves the instance running
set -u
. "$(dirname "$0")/emu.sh"
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
S=$(cd "$(dirname "$0")" && pwd)
NAME=$1; PORT=$2; MIN=${3:-60}
LIB=${LIB:-/tmp/stab1/build/muimaster.library}
FAST=${FAST:-8192}
MACH=a1200-31
OUT=${OUT:-$ZWORK/soak/$NAME.tsv}
mkdir -p "$(dirname "$OUT")" "$ZWORK/logs"
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
WASABI_BIN=$(type -P wasabi)
wasabi() { timeout 40 "$WASABI_BIN" "$@"; }
log() { echo "$(date +%T) $NAME: $*" >&2; }
boot() {
  amiga-test $MACH stop --as $NAME >/dev/null 2>&1
  ( amiga-test $MACH start --as $NAME --port $PORT --config fast_memory=$FAST --config fpu=68882 > "$ZWORK/logs/$NAME.log" 2>&1 < /dev/null & )
  local i
  for i in $(seq 1 60); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done
  for i in $(seq 1 30); do wasabi windows 2>/dev/null | grep -q '^ *window "Workbench"' && break; sleep 1; done
  sleep 2
}
avail() { wasabi run avail 2>/dev/null | awk '/^total/{print $3}'; }
flushed() { wasabi run "avail flush" >/dev/null; sleep 1; wasabi run "avail flush" >/dev/null; sleep 1; avail; }
nwin() { wasabi windows 2>/dev/null | grep -c '^ *window'; }
wline() { # TASK: the first window line of task RAM:TASK (MUI-Demo: its main window)
  if [ "$1" = MUI-Demo ]; then wasabi windows 2>/dev/null | grep 'window "MUI-Demo"' | head -1
  else wasabi windows 2>/dev/null | grep "task RAM:$1" | head -1; fi; }
title() { echo "$1" | sed 's/^ *window "\([^"]*\)".*/\1/'; }
geomof() { echo "$1" | sed 's/.* at \([0-9]*\),\([0-9]*\)  \([0-9]*\)x\([0-9]*\).*/\1 \2 \3 \4/'; }
geom() { local w; w=$(wline "$1"); [ -n "$w" ] && geomof "$w"; }
click() { wasabi mouse click "$1" "$2" >/dev/null 2>&1; }
front() { wasabi ui front "$1" >/dev/null 2>&1; }
keys() { local k; for k in "$@"; do wasabi key press "$k" >/dev/null 2>&1; done; }
closewin() { # TASK: the close gadget of its first window
  local g; g=$(geom "$1"); [ -n "$g" ] || return 1
  set -- $g; click $(($1 + 6)) $(($2 + 5)); sleep 1
}
closeline() { # WINDOW-LINE: its close gadget
  local g; g=$(geomof "$1"); set -- $g; click $(($1 + 6)) $(($2 + 5)); sleep 1
}
start_demo() { # NAME (already in RAM:)
  wasabi run "stack 40000
run >nil: RAM:$1" >/dev/null
  local i; for i in $(seq 1 60); do [ "$(wasabi windows 2>/dev/null | grep -c "task RAM:$1")" -ge 1 ] && break; sleep 0.3; done
  sleep 2
}
# MUI-Demo's buttons, relative to its bottom left corner (subwin.sh)
declare -A pos=( [Groups]="54 111" [Frames]="142 111" [Backfill]="230 111" [Notify]="54 81" [Listviews]="142 81" [Cycle]="230 81" [Images]="54 51" [Strings]="142 51" )
subwin() { # BUTTON: open, work, close
  local b=$1 g w x y ww hh t
  g=$(geom MUI-Demo); [ -n "$g" ] || return
  set -- $g; set -- $1 $2 $3 $4 ${pos[$b]}
  click $(($1 + $5)) $(($2 + $4 - $6)); sleep 2
  w=$(wasabi windows 2>/dev/null | grep 'task RAM:MUI-Demo' | grep -v 'window "MUI-Demo"' | head -1)
  [ -n "$w" ] || return
  t=$(title "$w")
  read x y ww hh < <(geomof "$w")
  front "$t"
  case $b in
    Listviews) click $((x + ww / 4)) $((y + hh / 2)); keys down down down down up up pagedown pageup; click $((x + ww / 2 - 8)) $((y + hh - 24)); click $((x + ww / 2 - 8)) $((y + hh - 24)) ;;
    Strings)   click $((x + ww / 2)) $((y + hh / 3)); wasabi key type "soak $RANDOM" >/dev/null 2>&1; keys return tab tab; wasabi key type "x" >/dev/null 2>&1; keys return ;;
    Cycle)     click $((x + ww / 2)) $((y + hh / 4)); click $((x + ww / 2)) $((y + hh / 4)); keys tab down down up ;;
    Groups|Frames|Backfill|Notify|Images) keys tab tab return tab ;;
  esac
  sleep 1; closeline "$w"
}
pages() { front "$(title "$(wline Pages)")"; keys tab right right right left left tab; }
virtual() { local g; g=$(geom Virtual); [ -n "$g" ] || return; set -- $g; click $(($1 + $3 - 10)) $(($2 + $4 - 26)); click $(($1 + $3 - 10)) $(($2 + $4 - 26)); click $(($1 + $3 - 24)) $(($2 + $4 - 8)); click $(($1 + $3 - 10)) $(($2 + 40)); click $(($1 + 24)) $(($2 + $4 - 8)); }
slidorama() { front "$(title "$(wline Slidorama)")"; keys tab right right right left tab down down up; }
iconify() { # MUI-Demo iconified and shown again through its ARexx port (the title gadget is
  # off at default settings; HIDE and SHOW are MUI's built-in ARexx commands)
  wasabi arexx MUIDEMO.1 HIDE >/dev/null 2>&1; sleep 2
  wasabi arexx MUIDEMO.1 SHOW >/dev/null 2>&1; sleep 2
  [ -n "$(geom MUI-Demo)" ] || log "MUI-Demo did not come back from iconify"
}
resize() { local g; g=$(geom MUI-Demo); [ -n "$g" ] || return; set -- $g
  drag $(($1 + $3 - 4)) $(($2 + $4 - 4)) $(($1 + $3 + 60)) $(($2 + $4 + 30)); sleep 1
  drag $(($1 + $3 + 56)) $(($2 + $4 + 26)) $(($1 + $3 - 4)) $(($2 + $4 - 4)); sleep 1; }
drag() { wasabi mouse move "$1" "$2" >/dev/null 2>&1; wasabi mouse down >/dev/null 2>&1; wasabi mouse move $(( ($1 + $3) / 2 )) $(( ($2 + $4) / 2 )) >/dev/null 2>&1; wasabi mouse move "$3" "$4" >/dev/null 2>&1; wasabi mouse up >/dev/null 2>&1; }

boot; "$S/putlib.sh" "$LIB" >/dev/null; boot
for d in MUI-Demo Pages Virtual Slidorama; do wasabi put "$ZWORK/demos/$d" "RAM:$d" >/dev/null; wasabi run "protect RAM:$d +e" >/dev/null; done
A0=$(flushed)
start_demo MUI-Demo; start_demo Pages; start_demo Virtual; start_demo Slidorama
: > "$OUT"
T0=$SECONDS; c=0
log "soak for $MIN min: $(nwin) windows, in use $(avail)"
while [ $(( (SECONDS - T0) / 60 )) -lt "$MIN" ]; do
  c=$((c + 1))
  for b in Groups Frames Backfill Notify Listviews Cycle Images Strings; do subwin $b; done
  pages; virtual; slidorama
  [ $((c % 3)) = 0 ] && iconify
  [ $((c % 2)) = 0 ] && resize
  wasabi ping >/dev/null 2>&1 || { log "cycle $c: the machine does not answer"; printf '%s\t%s\tdead\t\n' "$c" $((SECONDS - T0)) >> "$OUT"; break; }
  printf '%s\t%s\t%s\t%s\n' "$c" $((SECONDS - T0)) "$(avail)" "$(nwin)" | tee -a "$OUT" >&2
done
for d in Slidorama Virtual Pages MUI-Demo; do closewin "$d"; sleep 2; done
for i in $(seq 1 20); do wasabi ps 2>/dev/null | grep -q 'RAM:' || break; sleep 1; done
A1=$(flushed)
log "after $c cycles: tasks left $(wasabi ps 2>/dev/null | grep -c 'RAM:'), lost $((A1 - A0)) bytes (in use after - before)"
echo "lost	$((A1 - A0))" >> "$OUT"
[ -n "${KEEP:-}" ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1
