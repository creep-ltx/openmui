#!/usr/bin/env bash
# failsweep-par.sh NAME PORT0 PROG K N0 N1: failsweep.sh split over K instances NAME-1..K on
# ports PORT0..PORT0+K-1, each taking every Kth N (interleaved, so that every instance sees the
# whole start-up); the lines are merged, sorted by N, into $ZWORK/failsweep/NAME.tsv when all
# are done. The environment of failsweep.sh (LIB, FAILN, MASK, WAIT, FAST) passes through.
# N1 = all: a count run first (one instance), then the sweep 1..count.
set -u
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
S=$(cd "$(dirname "$0")" && pwd)
NAME=$1; PORT0=$2; PROG=$3; K=$4; N0=$5; N1=$6
mkdir -p "$ZWORK/failsweep"
if [ "$N1" = all ]; then
  N1=$("$S/failsweep.sh" "$NAME-count" "$PORT0" "$PROG" count all 2>&1 | grep -o 'counted [0-9]*' | awk '{print $2}')
  [ -n "$N1" ] || { echo "failsweep-par: no count" >&2; exit 1; }
  echo "$(date +%T) $NAME: $N1 calls to sweep" >&2
fi
pids=()
for i in $(seq 1 "$K"); do
  OUT=$ZWORK/failsweep/$NAME-$i.tsv; : > "$OUT"
  OUT=$OUT "$S/failsweep.sh" "$NAME-$i" $((PORT0 + i - 1)) "$PROG" $((N0 + i - 1)) "$N1" "$K" > "$ZWORK/logs/$NAME-$i.out" 2>&1 &
  pids+=($!)
  sleep 2
done
wait "${pids[@]}"
sort -n "$ZWORK"/failsweep/"$NAME"-[0-9]*.tsv > "$ZWORK/failsweep/$NAME.tsv"
echo "$(date +%T) $NAME: done, $(wc -l < "$ZWORK/failsweep/$NAME.tsv") lines in $ZWORK/failsweep/$NAME.tsv" >&2
awk -F'\t' '$2 != "ok" && $2 != "nowindow" {print}' "$ZWORK/failsweep/$NAME.tsv" | head -50
