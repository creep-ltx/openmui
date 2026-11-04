#!/usr/bin/env bash
# ZWORK = scratch folder with demos/ (default ~/.cache/zune68-lean): the MUI 3.8 demo binaries
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
# wintimes.sh NAME MACHINE PORT [LIBFILE]: cold boot (with LIBFILE and its class files), then
# start MUI-Demo three times, each while the earlier ones run, and print the seconds from "run"
# to its window. The second and third start with everything loaded: the difference to the first
# is loading, the rest is creating the objects and opening the window. EXTRA = FS-UAE options.
NAME=$1; MACH=$2; PORT=$3; LIB=${4:-}
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
boot() { amiga-test $MACH stop --as $NAME >/dev/null 2>&1; ( amiga-test $MACH start --as $NAME --port $PORT ${EXTRA:-} >/dev/null 2>&1 < /dev/null & ); for i in $(seq 1 40); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3; }
boot; if [ -n "$LIB" ]; then "$(dirname "$0")/putlib.sh" "$LIB" >/dev/null; boot; fi
wasabi put $ZWORK/demos/MUI-Demo RAM:Prog >/dev/null; wasabi run "protect RAM:Prog +e" >/dev/null; sleep 5
for n in 1 2 3; do
  T0=$(date +%s.%N)
  wasabi run "stack 40000
run >nil: RAM:Prog" >/dev/null
  for i in $(seq 1 600); do c=$(wasabi windows 2>/dev/null | grep -c "task RAM:Prog"); [ "$c" -ge $n ] && break; sleep 0.05; done
  T1=$(date +%s.%N); echo "$NAME window $n: $(python3 -c "print(round($T1-$T0,2))") s"; sleep 4
done
amiga-test $MACH stop --as $NAME >/dev/null 2>&1
