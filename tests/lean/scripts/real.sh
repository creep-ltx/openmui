#!/usr/bin/env bash
# real.sh TAG NAME MACH PORT LIB: the older real-program check (NList, NListtree, TextEditor and
# TheBar demos, YAM 2.5) on library LIB, now run by suite.sh (the whole suite: suite.sh zune
# --lib LIB). MACH must be a1200-31 (the suite's Zune machine). Results:
# $ZWORK/suite-real/real-TAG/results.tsv and screenshots there.
S=$(cd "$(dirname "$0")" && pwd)
TAG=$1; NAME=$2; MACH=$3; PORT=$4; LIB=$5
[ "$MACH" = a1200-31 ] || { echo "real.sh: the suite runs Zune on a1200-31" >&2; exit 2; }
exec "$S/suite.sh" zune --lib "$LIB" --name "$NAME" --port "$PORT" --tag "real-$TAG" \
  nlist-demo nlisttree-demo texteditor-demo thebar-demo1 thebar-demo2 thebar-demo3 thebar-demo4 \
  thebar-demo5 thebar-demo6 thebar-demo7 thebar-demo8 thebar-demo9 thebar-demo10 yam25
