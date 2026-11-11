#!/usr/bin/env bash
# full.sh TAG NAME MACH PORT LIB EXTRA: fullcheck.sh on a freshly booted instance with LIB installed
# (EMU=amiberry|fsuae: the emulator, default amiberry; EXTRA: amiga-test options, e.g. --config ...)
export ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
. "$(dirname "$0")/emu.sh"   # EMU=amiberry|fsuae: which emulator boots the machine
S=$(cd "$(dirname "$0")" && pwd)
TAG=$1; NAME=$2; MACH=$3; PORT=$4; LIB=$5; EXTRA=$6
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
boot() { amiga-test $MACH stop --as $NAME >/dev/null 2>&1; ( amiga-test $MACH start --as $NAME --port $PORT $EXTRA > $ZWORK/logs/$NAME.log 2>&1 < /dev/null & ); for i in $(seq 1 40); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3; }
boot; $S/putlib.sh $LIB; boot
$S/fullcheck.sh $TAG $PORT > $ZWORK/res/full-$TAG.txt 2>&1
wasabi windows >> $ZWORK/res/full-$TAG.txt 2>&1
wasabi info >> $ZWORK/res/full-$TAG.txt 2>&1
amiga-test $MACH stop --as $NAME >/dev/null 2>&1
