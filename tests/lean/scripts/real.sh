#!/usr/bin/env bash
# real.sh TAG NAME MACH PORT LIB: cold boot an own instance with 4 MB fast and a 68882 (YAM 2.5's
# 68k build needs an FPU), install LIB, the third-party classes and programs (realinstall*.sh,
# downloads in $REAL, default ~/.cache/zune68k-real), reboot, run each with realrun.sh.
# Results: $ZWORK/res/real-TAG.txt, screenshots $ZWORK/real/.
export ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
S=$(cd "$(dirname "$0")" && pwd)
TAG=$1; NAME=$2; MACH=$3; PORT=$4; LIB=$5
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
boot() { amiga-test $MACH stop --as $NAME >/dev/null 2>&1
  ( amiga-test $MACH start --as $NAME --port $PORT --config fast_memory=4096 --config fpu=68882 > $ZWORK/logs/$NAME.log 2>&1 < /dev/null & )
  for i in $(seq 1 40); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 3
  for i in $(seq 1 30); do wasabi windows 2>/dev/null | grep -q '^screen "Workbench Screen"' && break; sleep 1; done; sleep 2; }
boot; $S/putlib.sh $LIB; $S/realinstall.sh $PORT; $S/realinstall2.sh $PORT >/dev/null; boot
{ for p in NList-Demo NListtree-Demo TextEditor-Demo-AmigaOS3 demo1-AmigaOS3 demo2-AmigaOS3 demo3-AmigaOS3 \
    demo4-AmigaOS3 demo5-AmigaOS3 demo6-AmigaOS3 demo7-AmigaOS3 demo8-AmigaOS3 demo9-AmigaOS3 demo10-AmigaOS3; do
    $S/realrun.sh $TAG $PORT $p; done
  WAIT=120 SETTLE=10 $S/realrun.sh $TAG $PORT YAM/YAM
  wasabi info | grep -i -e guru -e alert
} > $ZWORK/res/real-$TAG.txt 2>&1
amiga-test $MACH stop --as $NAME >/dev/null 2>&1
