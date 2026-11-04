#!/usr/bin/env bash
# ZWORK = scratch folder with demos/, logs/, suite/ (default ~/.cache/zune68-lean): the MUI 3.8 demo binaries go in demos/
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
# subwin.sh BUTTON...: in a running MUI-Demo, click each demo button (Groups Frames Backfill Notify
# Listviews Cycle Images Strings), report the sub-window, screenshot it, close it again.
# Button places are relative to the MUI-Demo window's bottom left corner (same on MUI 3.8 and Zune68k; on Zune68 main ROW1=126 ROW2=90 ROW3=54).
export WASABI_HOST=127.0.0.1 WASABI_PORT=${WASABI_PORT:-1303}
mkdir -p $ZWORK/suite
declare -A pos=( [Groups]="54 ${ROW1:-111}" [Frames]="142 ${ROW1:-111}" [Backfill]="230 ${ROW1:-111}" [Notify]="54 ${ROW2:-81}" [Listviews]="142 ${ROW2:-81}" [Cycle]="230 ${ROW2:-81}" [Images]="54 ${ROW3:-51}" [Strings]="142 ${ROW3:-51}" )
geom() { echo "$1" | sed 's/.* at \([0-9]*\),\([0-9]*\)  \([0-9]*\)x\([0-9]*\).*/\1 \2 \3 \4/'; }
for b in "$@"; do
  read mx my mw mh < <(geom "$(wasabi windows | grep '"MUI-Demo"' | head -1)")
  set -- ${pos[$b]}; wasabi mouse click $((mx + $1)) $((my + mh - $2)) >/dev/null; sleep 6
  w=$(wasabi windows | grep -v '"MUI-Demo"\|"Workbench"' | grep window | head -1)
  echo "$b: ${w:-no window}"
  f=$(wasabi grab 2>/dev/null | tail -1 | sed 's/.*-> //; s/ (.*//'); cp $f $ZWORK/suite/${TAG:-z}-MUIDemo-$b.png
  if [ -n "$w" ]; then read x y ww hh < <(geom "$w"); wasabi mouse click $((x+6)) $((y+5)) >/dev/null; sleep 3; fi
done
