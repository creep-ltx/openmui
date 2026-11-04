#!/usr/bin/env bash
# snoop.sh NAME MACH PORT LIB OUT EXTRA: cold boot, start MUI-Demo under wasabi debug --with-snoop
export ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
S=$(cd "$(dirname "$0")" && pwd)
NAME=$1; MACH=$2; PORT=$3; LIB=$4; OUT=$5; EXTRA=$6
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
boot() { amiga-test $MACH stop --as $NAME >/dev/null 2>&1; ( amiga-test $MACH start --as $NAME --port $PORT $EXTRA > $ZWORK/logs/$NAME.log 2>&1 < /dev/null & ); for i in $(seq 1 40); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3; }
boot; $S/putlib.sh $LIB; boot
wasabi put $ZWORK/demos/MUI-Demo RAM:Prog >/dev/null; wasabi run "protect RAM:Prog +e" >/dev/null
rm -f $OUT
wasabi debug --with-snoop --ignore-wasabi --output full --for 45 --log $OUT >/dev/null 2>&1 &
DP=$!
sleep 3
wasabi run "stack 40000
run >nil: RAM:Prog" >/dev/null
for i in $(seq 1 600); do n=$(wasabi windows 2>/dev/null | grep -c "task RAM:Prog"); [ "$n" -ge 1 ] && break; sleep 0.1; done
sleep 5
wasabi put $ZWORK/libs RAM:libs >/dev/null; wasabi run "protect RAM:libs +e" >/dev/null
wasabi run RAM:libs > $OUT.libs
wait $DP
amiga-test $MACH stop --as $NAME >/dev/null 2>&1
