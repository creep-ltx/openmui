#!/usr/bin/env bash
# probe.sh PROG.c MACH PORT [LIB]: build PROG.c, boot MACH (with LIB installed when given), run it, print its output
export PATH=$HOME/opt/amiga-gcc16/bin:$PATH
W=$(cd "$(dirname "$0")/../../.." && pwd)
cd $W || exit 1
. tests/lean/scripts/emu.sh
SRC=$1; MACH=$2; PORT=$3; LIB=${4:-}
B=${TMPDIR:-/tmp}/probe-$(basename "$SRC" .c)
m68k-amigaos-gcc -Os -m68000 -noixemul -s -o "$B" "$SRC" || exit 1
NAME=pr-$PORT
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
boot() { amiga-test $MACH stop --as $NAME >/dev/null 2>&1
  ( amiga-test $MACH start --as $NAME --port $PORT --config fast_memory=8192 --config fpu=68882 >/dev/null 2>&1 < /dev/null & )
  for i in $(seq 1 60); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done; sleep 4; }
boot
[ -n "$LIB" ] && { tests/lean/scripts/putlib.sh "$LIB" >/dev/null; boot; }
wasabi put "$B" RAM:probe >/dev/null; wasabi run "protect RAM:probe +e" >/dev/null
wasabi run "RAM:probe"
[ -n "${KEEP:-}" ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1
