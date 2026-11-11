#!/usr/bin/env bash
# ZWORK = scratch folder with demos/, logs/, suite/ (default ~/.cache/zune68-lean): the MUI 3.8 demo binaries go in demos/
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
. "$(dirname "$0")/emu.sh"   # EMU=amiberry|fsuae: which emulator boots the machine
# libonly.sh NAME MACHINE PORT [LIBFILE]: memory taken by just opening muimaster.library, cold
set -u
NAME=$1; MACH=$2; PORT=$3; LIB=${4:-}
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
boot() { amiga-test $MACH stop --as $NAME >/dev/null 2>&1; ( amiga-test $MACH start --as $NAME --port $PORT ${EXTRA:-} > $ZWORK/logs/$NAME.log 2>&1 < /dev/null & ); for i in $(seq 1 40); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3; }
boot
if [ -n "$LIB" ]; then "$(dirname "$0")/putlib.sh" "$LIB"; boot; fi
wasabi put $ZWORK/HoldLib RAM:HoldLib >/dev/null
wasabi run "avail flush" >/dev/null; sleep 1
B0=$(wasabi run avail | awk '/^total/{print $3}')
wasabi run "run >nil: RAM:HoldLib" >/dev/null; sleep 5
B1=$(wasabi run avail | awk '/^total/{print $3}')
echo "$NAME library open only: $((B1-B0))"
