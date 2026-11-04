#!/usr/bin/env bash
# m.sh TAG NAME MACH PORT LIB EXTRA : measure + libonly + libsopen, results to res/TAG.txt
export ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
S=$(cd "$(dirname "$0")" && pwd)
TAG=$1; NAME=$2; MACH=$3; PORT=$4; LIB=$5; export EXTRA=$6
R=$ZWORK/res/$TAG.txt
{ echo "== $TAG lib=$LIB mach=$MACH extra=$EXTRA $(date)"
  $S/measure.sh $NAME $MACH $PORT MUI-Demo $LIB
  $S/measure.sh $NAME $MACH $PORT MUI-Demo $LIB
  $S/libonly.sh $NAME $MACH $PORT $LIB
  $S/libsopen.sh $NAME $MACH $PORT MUI-Demo $LIB
  amiga-test $MACH stop --as $NAME
} > $R 2>&1
