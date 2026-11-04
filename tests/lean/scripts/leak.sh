#!/usr/bin/env bash
# ZWORK = scratch folder with demos/, logs/, suite/ (default ~/.cache/zune68-lean): the MUI 3.8 demo binaries go in demos/
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
# leak.sh NAME MACHINE PORT DEMO... : on a running instance (library already installed),
# for each DEMO: Avail (flushed) before, start it, screenshot, close its window(s) with the
# close gadget, Avail (flushed) after. Prints the difference; 0 = nothing lost.
set -u
NAME=$1; MACH=$2; PORT=$3; shift 3
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
mkdir -p $ZWORK/suite
avail() { wasabi run "avail flush" >/dev/null; sleep 1; wasabi run "avail flush" >/dev/null; sleep 1; wasabi run avail | awk '/^total/{print $3}'; }
for DEMO in "$@"; do
  wasabi put $ZWORK/demos/$DEMO RAM:$DEMO >/dev/null; wasabi run "protect RAM:$DEMO +e" >/dev/null
  A0=$(avail)
  wasabi run "stack 40000
run >nil: RAM:$DEMO" >/dev/null
  for i in $(seq 1 100); do n=$(wasabi windows 2>/dev/null | grep -c "task RAM:$DEMO"); [ "$n" -ge 1 ] && break; sleep 0.3; done
  sleep 4
  WINS=$(wasabi windows | grep "task RAM:$DEMO" | sed 's/^ *window //; s/  task.*//' | tr '\n' ';')
  f=$(wasabi grab 2>/dev/null | tail -1 | sed 's/.*-> //; s/ (.*//'); cp "$f" $ZWORK/suite/$NAME-$DEMO.png
  for k in 1 2 3 4 5 6; do
    w=$(wasabi windows | grep "task RAM:$DEMO" | head -1)
    [ -z "$w" ] && break
    x=$(echo "$w" | sed 's/.* at \([0-9]*\),\([0-9]*\).*/\1/'); y=$(echo "$w" | sed 's/.* at \([0-9]*\),\([0-9]*\).*/\2/')
    wasabi mouse click $((x+6)) $((y+5)) >/dev/null; sleep 3
  done
  for i in $(seq 1 20); do wasabi ps 2>/dev/null | grep -q "RAM:$DEMO" || break; sleep 1; done
  left=$(wasabi ps 2>/dev/null | grep -c "RAM:$DEMO")
  A1=$(avail)
  echo "$DEMO: windows [$WINS] still running=$left lost=$((A1-A0))"
done
