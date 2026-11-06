#!/usr/bin/env bash
# demopix.sh: the MUI 3.8 demo set on Zune and on MUI 3.8, compared pixel by pixel (roadmap phase 2).
#
#   demopix.sh [options] [SHOT...]
#   --lib FILE     Zune's muimaster.library (default /tmp/suite/build/muimaster.library; its
#                  class files from Zune/ next to it)
#   --zport N      wasabid port of the Zune instance (default 1442), --mport N MUI 3.8's (1443)
#   --only T       shoot only zune or mui38 (the other side's shots from an earlier run stay)
#   --compare      shoot nothing, only compare what is there
#   --out DIR      results (default $ZWORK/demopix)
#
# SHOT: a demo name (Class1 ... BoopsiDoor, MUI-Demo) or MUIDemo-<button> (Groups Frames Backfill
# Notify Listviews Cycle Images Strings); none = all 27. Each side runs on its own freshly booted
# instance: a1200-31 with the library (Zune) and a1200-31-mui38 (MUI 3.8), both with 8 MB fast
# RAM, default settings (no ENV:MUI, no ENV:Zune), the same 640x256 Workbench screen and fonts.
# For each shot: start the demo (stack 40000), wait for its windows, wait until the screen stops
# changing, grab the screen unscaled (640x256) and write the window list; then close it. The
# compare (demopix-compare.py) crops each window, matches them by title and writes
# OUT/report.html with both windows side by side, a difference picture and the number of
# differing pixels per window.
set -u
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
S=$(cd "$(dirname "$0")" && pwd)
DEMOS=${DEMOS:-$HOME/.cache/zune-eval/demos}
LIB=/tmp/suite/build/muimaster.library; ZPORT=1442; MPORT=1443; ONLY=; CMPONLY=0; OUT=$ZWORK/demopix; SHOTS=()
while [ $# -gt 0 ]; do
  case "$1" in
    --lib) LIB=$2; shift ;; --zport) ZPORT=$2; shift ;; --mport) MPORT=$2; shift ;;
    --only) ONLY=$2; shift ;; --compare) CMPONLY=1 ;; --out) OUT=$2; shift ;;
    -*) echo "demopix.sh: unknown option $1" >&2; exit 2 ;;
    *) SHOTS+=("$1") ;;
  esac; shift
done
ALL_DEMOS="Class1 Class2 Class3 Pages Layout Popup Menus DragnDrop ShowHide Virtual Balancing
  Slidorama Subtask InputHandler EnvBrowser WbMan AppWindow BoopsiDoor MUI-Demo"
ALL_SUB="Groups Frames Backfill Notify Listviews Cycle Images Strings"
if [ ${#SHOTS[@]} -eq 0 ]; then
  SHOTS=($ALL_DEMOS); for b in $ALL_SUB; do SHOTS+=("MUIDemo-$b"); done
fi
mkdir -p "$OUT/zune" "$OUT/mui38" "$OUT/logs"
log() { echo "$(date +%T) $*" | tee -a "$OUT/logs/demopix.log" >&2; }

boot() { # MACHINE NAME PORT [--fresh]
  amiga-test "$1" stop --as "$2" >/dev/null 2>&1
  # an instance of someone else on the port must not be shot instead
  if ! amiga-test "$1" start --as "$2" --port "$3" ${4:-} --config 'fast_memory = 8192' \
      > "$OUT/logs/$2.log" 2>&1 < /dev/null; then
    log "$2 did not start: $(tail -1 "$OUT/logs/$2.log")"; exit 1
  fi
  local i
  for i in $(seq 1 30); do wasabi windows 2>/dev/null | grep -q '^ *window "Workbench"' && break; sleep 1; done
  sleep 2
}
# the windows of a task: raw lines "W addr x y w h ... task title"
taskwins() { wasabi windows --raw 2>/dev/null | awk -F'\t' -v t="$1" '$1=="W" && $(NF-1)==t'; }
# grab until two grabs in a row are the same (at most N tries), into FILE
grab_settled() { # FILE
  local prev=$OUT/logs/prev-$SIDE.png i
  wasabi grab --raw "$prev" >/dev/null 2>&1
  for i in 1 2 3 4 5 6; do
    sleep 1.5
    wasabi grab --raw "$1" >/dev/null 2>&1
    cmp -s "$1" "$prev" && return 0
    cp "$1" "$prev"
  done
  log "  $SIDE $(basename "$1" .png): the screen still changes after 6 grabs (animated?)"
  return 0
}
close_task() { # TASK
  local k w x y
  for k in 1 2 3 4 5 6; do
    w=$(taskwins "$1" | head -1); [ -z "$w" ] && break
    x=$(echo "$w" | cut -f3); y=$(echo "$w" | cut -f4)
    wasabi mouse click $((x+6)) $((y*2 + 5)) >/dev/null; sleep 2   # mouse: rows doubled
  done
  for k in $(seq 1 10); do wasabi ps 2>/dev/null | grep -qF "$1" || return 0; sleep 1; done
  wasabi kill "$1" >/dev/null 2>&1; sleep 2
}
start_demo() { # DEMO: start RAM:DEMO, wait for its first window
  wasabi put "$DEMOS/$1" "RAM:$1" >/dev/null 2>&1; wasabi run "protect RAM:$1 +e" >/dev/null
  wasabi run "stack 40000
run >nil: RAM:$1" >/dev/null
  local i; for i in $(seq 1 100); do [ -n "$(taskwins "RAM:$1")" ] && return 0; sleep 0.3; done
  return 1
}

shoot() { # SIDE MACHINE NAME PORT
  local side=$1 mach=$2 name=$3 d=$OUT/$1 s demo sub; SIDE=$1
  export WASABI_HOST=127.0.0.1 WASABI_PORT=$4
  boot "$mach" "$name" "$4" --fresh
  if [ "$side" = zune ]; then
    "$S/putlib.sh" "$LIB" >/dev/null 2>&1
    wasabi run "delete ENVARC:Zune ENV:Zune ALL QUIET" >/dev/null 2>&1
    boot "$mach" "$name" "$4"
  fi
  wasabi run "delete ENV:MUI ENVARC:MUI ALL QUIET" >/dev/null 2>&1
  local subs=()
  for s in "${SHOTS[@]}"; do
    case "$s" in MUIDemo-*) subs+=("${s#MUIDemo-}"); continue ;; esac
    log "$side: $s"
    if ! start_demo "$s"; then log "  no window"; : > "$d/$s.wins"; continue; fi
    grab_settled "$d/$s.png"
    taskwins "RAM:$s" > "$d/$s.wins"
    close_task "RAM:$s"
  done
  if [ ${#subs[@]} -gt 0 ]; then
    # MUI-Demo's sub-windows: its buttons, relative to the window's bottom left corner (as subwin.sh)
    declare -A pos=( [Groups]="54 111" [Frames]="142 111" [Backfill]="230 111" [Notify]="54 81"
      [Listviews]="142 81" [Cycle]="230 81" [Images]="54 51" [Strings]="142 51" )
    start_demo MUI-Demo || log "MUI-Demo: no window"
    sleep 3
    local main mx my mh b w
    for b in "${subs[@]}"; do
      log "$side: MUIDemo-$b"
      main=$(taskwins RAM:MUI-Demo | awk -F'\t' '$NF=="MUI-Demo"' | head -1)
      mx=$(echo "$main" | cut -f3); my=$(echo "$main" | cut -f4); mh=$(echo "$main" | cut -f6)
      set -- ${pos[$b]}
      wasabi mouse click $((mx + $1)) $(( (my + mh) * 2 - $2 )) >/dev/null
      local i; for i in $(seq 1 30); do
        w=$(taskwins RAM:MUI-Demo | awk -F'\t' '$NF!="MUI-Demo"'); [ -n "$w" ] && break; sleep 0.5; done
      grab_settled "$d/MUIDemo-$b.png"
      taskwins RAM:MUI-Demo | awk -F'\t' '$NF!="MUI-Demo"' > "$d/MUIDemo-$b.wins"
      [ -s "$d/MUIDemo-$b.wins" ] || log "  no window"
      w=$(head -1 "$d/MUIDemo-$b.wins")
      if [ -n "$w" ]; then
        wasabi mouse click $(( $(echo "$w" | cut -f3) + 6 )) $(( $(echo "$w" | cut -f4) * 2 + 5 )) >/dev/null
        for i in $(seq 1 20); do [ -z "$(taskwins RAM:MUI-Demo | awk -F'\t' '$NF!="MUI-Demo"')" ] && break; sleep 0.5; done
      fi
    done
    close_task RAM:MUI-Demo
  fi
  wasabi info 2>/dev/null | grep -i -e guru -e alert >> "$OUT/logs/$side-alerts.txt"
  amiga-test "$mach" stop --as "$name" >/dev/null 2>&1
}

if [ "$CMPONLY" = 0 ]; then
  [ -z "$ONLY" ] || [ "$ONLY" = zune ] && { [ -f "$LIB" ] || { echo "demopix.sh: no $LIB" >&2; exit 1; }; }
  if [ -z "$ONLY" ]; then
    shoot zune a1200-31 demopix-zune "$ZPORT" &
    shoot mui38 a1200-31-mui38 demopix-mui38 "$MPORT" &
    wait
  else
    case "$ONLY" in
      zune) shoot zune a1200-31 demopix-zune "$ZPORT" ;;
      mui38) shoot mui38 a1200-31-mui38 demopix-mui38 "$MPORT" ;;
      *) echo "demopix.sh: --only zune|mui38" >&2; exit 2 ;;
    esac
  fi
fi
python3 "$S/demopix-compare.py" "$OUT"
