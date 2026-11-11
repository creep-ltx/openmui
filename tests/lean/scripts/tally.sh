#!/usr/bin/env bash
# tally.sh NAME MACHINE PORT DEMO OUT [LIBFILE]: cold boot, (install LIBFILE), start DEMO
# once normally, then a second copy under memtally; dump the second copy's live blocks to OUT.
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
. "$(dirname "$0")/emu.sh"   # EMU=amiberry|fsuae: which emulator boots the machine
set -u
NAME=$1; MACH=$2; PORT=$3; DEMO=$4; OUT=$5; LIB=${6:-}; STK=${STK:-40000}; FIRST=${FIRST:-1}
D=$(cd "$(dirname "$0")" && pwd)
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
boot() { amiga-test $MACH stop --as $NAME >/dev/null 2>&1
  ( amiga-test $MACH start --as $NAME --port $PORT ${EXTRA:-} > $ZWORK/logs/$NAME.log 2>&1 < /dev/null & )
  for i in $(seq 1 40); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3; }
boot
if [ -n "$LIB" ]; then "$D/putlib.sh" "$LIB"; boot; fi
wasabi put $ZWORK/demos/$DEMO RAM:Prog >/dev/null; wasabi run "protect RAM:Prog +e" >/dev/null
wasabi put "${MEMTALLY:-$(dirname "$0")/../memtally/memtally}" RAM:memtally >/dev/null; wasabi run "protect RAM:memtally +e" >/dev/null
if [ "$FIRST" = 1 ]; then
wasabi run "stack $STK
run >nil: RAM:Prog" >/dev/null
for i in $(seq 1 600); do n=$(wasabi windows 2>/dev/null | grep -c "task RAM:Prog"); [ "$n" -ge 1 ] && break; sleep 0.1; done
sleep 4; fi
wasabi run "avail flush" >/dev/null; sleep 1
B0=$(wasabi run avail | awk '/^total/{print $3}')
wasabi run "stack $STK
run >nil: RAM:memtally run RAM:Prog" >/dev/null
for i in $(seq 1 600); do n=$(wasabi windows 2>/dev/null | grep -c "task RAM:memtally\|task RAM:Prog"); [ "$n" -ge $((FIRST+1)) ] && break; sleep 0.1; done
sleep 5
B1=$(wasabi run avail | awk '/^total/{print $3}')
wasabi run "RAM:memtally dump RAM:mt.txt"
wasabi get RAM:mt.txt "$OUT" >/dev/null
echo "avail delta $((B1-B0))"
wasabi windows
