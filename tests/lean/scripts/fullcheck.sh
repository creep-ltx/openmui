#!/usr/bin/env bash
# ZWORK = scratch folder with demos/, logs/, suite/ (default ~/.cache/zune68-lean): the MUI 3.8 demo binaries go in demos/
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
# fullcheck.sh TAG PORT: on a freshly booted instance (library installed), MUI-Demo with all
# eight sub-windows, then the other 18 MUI 3.8 demos; each opened, screenshot, closed, and the
# memory difference (Avail after "avail flush") printed. Screenshots: $ZWORK/suite/TAG-*.png
set -u
TAG=$1; PORT=$2
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
D=$(dirname "$0")
avail() { wasabi run "avail flush" >/dev/null; sleep 1; wasabi run "avail flush" >/dev/null; sleep 1; wasabi run avail | awk '/^total/{print $3}'; }
wasabi put $ZWORK/demos/MUI-Demo RAM:MUI-Demo >/dev/null; wasabi run "protect RAM:MUI-Demo +e" >/dev/null
A0=$(avail)
wasabi run "stack 40000
run >nil: RAM:MUI-Demo" >/dev/null
for i in $(seq 1 100); do wasabi windows 2>/dev/null | grep -q '"MUI-Demo"' && break; sleep 0.3; done
sleep 4
TAG=$TAG "$D/subwin.sh" Groups Frames Backfill Notify Listviews Cycle Images Strings
w=$(wasabi windows | grep '"MUI-Demo"' | head -1)
x=$(echo "$w" | sed 's/.* at \([0-9]*\),\([0-9]*\).*/\1/'); y=$(echo "$w" | sed 's/.* at \([0-9]*\),\([0-9]*\).*/\2/')
wasabi mouse click $((x+6)) $((y+5)) >/dev/null
for i in $(seq 1 20); do wasabi ps 2>/dev/null | grep -q "RAM:MUI-Demo" || break; sleep 1; done
A1=$(avail)
echo "MUI-Demo with 8 sub-windows: still running=$(wasabi ps 2>/dev/null | grep -c RAM:MUI-Demo) lost=$((A1-A0))"
"$D/leak.sh" $TAG x $PORT Class1 Class2 Class3 Pages Layout Popup Menus DragnDrop ShowHide Virtual \
  Balancing Slidorama Subtask InputHandler EnvBrowser WbMan AppWindow BoopsiDoor 2>&1 | grep -v -- "->"
