#!/usr/bin/env bash
# realrun.sh TAG PORT PROG [ARGS]: run DH0:Real/PROG (path relative to DH0:Real) on the
# instance at PORT twice. Run 1 (no flush before: on MUI 3.8 a cold "avail flush" made NList-Demo's
# window fail to open): screenshot to $ZWORK/real/TAG-<name>.png, close its windows with the close
# gadget. Run 2 (LEAK=0: skipped): "avail flush" before and after, memory lost reported. KEEP=1: run 1 only, left open.
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
set -u
TAG=$1; PORT=$2; PROG=$3; shift 3; ARGS="$*"
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
mkdir -p $ZWORK/real
N=$(basename "$PROG")
DIR=$(dirname "DH0:Real/$PROG")
avail() { wasabi run "avail flush" >/dev/null; sleep 1; wasabi run "avail flush" >/dev/null; sleep 1; wasabi run avail | awk '/^total/{print $3}'; }
start() { wasabi run "cd $DIR
stack 40000
run >RAM:out-$N.txt DH0:Real/$PROG $ARGS" >/dev/null
  for i in $(seq 1 ${WAIT:-80}); do wasabi windows 2>/dev/null | grep -q "task DH0:Real/$PROG" && break; sleep 0.5; done
  sleep ${SETTLE:-5}; }
closeall() { for k in 1 2 3 4 5 6; do
    w=$(wasabi windows | grep "task DH0:Real/$PROG" | head -1)
    [ -z "$w" ] && break
    x=$(echo "$w" | sed 's/.* at \([0-9]*\),\([0-9]*\).*/\1/'); y=$(echo "$w" | sed 's/.* at \([0-9]*\),\([0-9]*\).*/\2/')
    wasabi mouse click $((x+6)) $((y+5)) >/dev/null; sleep 3
  done
  for i in $(seq 1 15); do wasabi ps 2>/dev/null | grep -q "DH0:Real/$PROG" || break; sleep 1; done; }
start
WINS=$(wasabi windows 2>/dev/null | grep "task DH0:Real/$PROG" | sed 's/^ *window //; s/  task.*//' | tr '\n' ';')
f=$(wasabi grab 2>/dev/null | tail -1 | sed 's/.*-> //; s/ (.*//'); cp "$f" $ZWORK/real/$TAG-$N.png 2>/dev/null
[ "${KEEP:-0}" = 1 ] && { echo "$N: windows [$WINS] (left running)"; exit 0; }
closeall
left=$(wasabi ps 2>/dev/null | grep -c "DH0:Real/$PROG")
out=$(wasabi run "type RAM:out-$N.txt" 2>/dev/null | grep -v '^\[CLI' | head -3 | tr '\n' '|')
if [ "${LEAK:-1}" = 1 ] && [ "$left" = 0 ] && [ -n "$WINS" ]; then
  A0=$(avail); start; closeall; A1=$(avail)
  L="lost=$((A0-A1)) (2nd run)"
else L="lost=?"; fi
echo "$N: windows [$WINS] still running=$left $L out=[$out]"
