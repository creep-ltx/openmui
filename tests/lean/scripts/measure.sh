#!/usr/bin/env bash
# ZWORK = scratch folder with demos/, logs/, suite/ (default ~/.cache/zune68-lean): the MUI 3.8 demo binaries go in demos/
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
# measure.sh NAME MACHINE PORT DEMOFILE [LIBFILE]: cold-boot an instance and measure
# memory for the first MUI-Demo, then two more copies of it, and time to first window.
set -u
NAME=$1; MACH=$2; PORT=$3; DEMO=$4; LIB=${5:-}; STK=${STK:-40000}
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
amiga-test $MACH stop --as $NAME >/dev/null 2>&1
( amiga-test $MACH start --as $NAME --port $PORT $EXTRA > $ZWORK/logs/$NAME.log 2>&1 < /dev/null & )
for i in $(seq 1 40); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done
sleep 3
[ -n "$LIB" ] && "$(dirname "$0")/putlib.sh" "$LIB"
wasabi put $ZWORK/demos/$DEMO RAM:Prog >/dev/null
wasabi run "protect RAM:Prog +e" >/dev/null
[ -n "$LIB" ] && { amiga-test $MACH stop --as $NAME >/dev/null 2>&1; ( amiga-test $MACH start --as $NAME --port $PORT $EXTRA > $ZWORK/logs/$NAME.log 2>&1 < /dev/null & ); for i in $(seq 1 40); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3; wasabi put $ZWORK/demos/$DEMO RAM:Prog >/dev/null; wasabi run "protect RAM:Prog +e" >/dev/null; }
used() { wasabi run "avail total" | tr -d ' \r'; }
wasabi run "avail flush" >/dev/null; sleep 1
B0=$(wasabi run avail | awk '/^total/{print $3}')
T0=$(date +%s.%N)
wasabi run "stack $STK
run >nil: RAM:Prog" >/dev/null
for i in $(seq 1 600); do n=$(wasabi windows 2>/dev/null | grep -c "task RAM:Prog"); [ "$n" -ge 1 ] && break; sleep 0.1; done
T1=$(date +%s.%N)
sleep 4
B1=$(wasabi run avail | awk '/^total/{print $3}')
wasabi run "stack $STK
run >nil: RAM:Prog" >/dev/null
for i in $(seq 1 600); do n=$(wasabi windows 2>/dev/null | grep -c "task RAM:Prog"); [ "$n" -ge 2 ] && break; sleep 0.1; done
sleep 4
B2=$(wasabi run avail | awk '/^total/{print $3}')
wasabi run "stack $STK
run >nil: RAM:Prog" >/dev/null
for i in $(seq 1 600); do n=$(wasabi windows 2>/dev/null | grep -c "task RAM:Prog"); [ "$n" -ge 3 ] && break; sleep 0.1; done
sleep 4
B3=$(wasabi run avail | awk '/^total/{print $3}')
echo "$NAME first=$((B1-B0)) second=$((B2-B1)) third=$((B3-B2)) firstwindow=$(python3 -c "print(round($T1-$T0,2))") s  (in-use $B0 $B1 $B2 $B3)"
wasabi windows | grep -c "task RAM:Prog"
