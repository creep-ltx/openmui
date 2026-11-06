#!/usr/bin/env bash
# libtally.sh NAME MACHINE PORT OUT [LIBFILE]: cold boot (install LIBFILE), then open
# muimaster.library under "memtally lib" (every task's AllocMem/FreeMem recorded, also
# ramlib's LoadSeg and the library's init) and dump the blocks still held to OUT.
# With PROG=<demo in $ZWORK/demos> the first copy of that program runs under "memtally
# runall" instead (the library, its class files, fonts and the program, all tasks).
# Read OUT with tools/lean/mtreport2.py OUT LIB tasks|cats|3.
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
set -u
NAME=$1; MACH=$2; PORT=$3; OUT=$4; LIB=${5:-}; STK=${STK:-40000}; PROG=${PROG:-}
D=$(cd "$(dirname "$0")" && pwd)
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
boot() { amiga-test $MACH stop --as $NAME >/dev/null 2>&1
  ( amiga-test $MACH start --as $NAME --port $PORT ${EXTRA:-} > $ZWORK/logs/$NAME.log 2>&1 < /dev/null & )
  for i in $(seq 1 40); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3; }
boot
if [ -n "$LIB" ]; then "$D/putlib.sh" "$LIB"; boot; fi
wasabi put "${MEMTALLY:-$ZWORK/memtally}" RAM:memtally >/dev/null; wasabi run "protect RAM:memtally +e" >/dev/null
[ -n "$PROG" ] && { wasabi put $ZWORK/demos/$PROG RAM:Prog >/dev/null; wasabi run "protect RAM:Prog +e" >/dev/null; }
wasabi run "avail flush" >/dev/null; sleep 1
B0=$(wasabi run avail | awk '/^total/{print $3}')
if [ -n "$PROG" ]; then
  wasabi run "stack $STK
run >nil: RAM:memtally runall RAM:Prog" >/dev/null
  for i in $(seq 1 600); do [ "$(wasabi windows 2>/dev/null | grep -c 'task RAM:memtally')" -ge 1 ] && break; sleep 0.1; done
  sleep 5
else
  wasabi run "run >nil: RAM:memtally lib muimaster.library 11" >/dev/null; sleep 5
fi
B1=$(wasabi run avail | awk '/^total/{print $3}')
wasabi run "RAM:memtally dump RAM:mt.txt"
wasabi get RAM:mt.txt "$OUT" >/dev/null
echo "avail delta $((B1-B0))"
