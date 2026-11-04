#!/usr/bin/env bash
# ZWORK = scratch folder with demos/, logs/, port/tests/ (default ~/.cache/zune68-lean): the MUI 3.8 demo binaries go in demos/
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
# libsopen.sh NAME MACHINE PORT DEMO [LIBFILE]: cold boot, start DEMO, list the libraries in memory
set -u
NAME=$1; MACH=$2; PORT=$3; DEMO=$4; LIB=${5:-}
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
boot() { amiga-test $MACH stop --as $NAME >/dev/null 2>&1; ( amiga-test $MACH start --as $NAME --port $PORT ${EXTRA:-} > $ZWORK/logs/$NAME.log 2>&1 < /dev/null & ); for i in $(seq 1 40); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3; }
boot
if [ -n "$LIB" ]; then "$(dirname "$0")/putlib.sh" "$LIB"; boot; fi
wasabi put $ZWORK/demos/$DEMO RAM:Prog >/dev/null
wasabi put $ZWORK/libs RAM:libs >/dev/null
wasabi run "protect RAM:Prog +e
protect RAM:libs +e" >/dev/null
wasabi run "stack 40000
run >nil: RAM:Prog" >/dev/null
for i in $(seq 1 600); do n=$(wasabi windows 2>/dev/null | grep -c "task RAM:Prog"); [ "$n" -ge 1 ] && break; sleep 0.1; done
sleep 3
wasabi run "RAM:libs"
