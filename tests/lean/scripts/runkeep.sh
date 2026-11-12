#!/usr/bin/env bash
# runkeep.sh NAME PORT ID [RUNS]: what a suite program keeps per run, and who allocated it. The
# program and its classes installed as suite.sh does (only that program); one warm-up run, then
# "memtally lib muimaster.library" (every task's AllocMem/FreeMem while OpenMUI is open, so the
# library stays loaded) and RUNS (default 3) start/quit cycles with "avail flush" after each and
# a memtally dump after each: OUT/ID-K.mt. The blocks of dump K that dump K-1 did not have are
# what run K kept: OUT/ID-kept.txt (by tools/lean/mtlib.py, grouped by first caller; SYMLIB=
# the unstripped library names OpenMUI's functions). Also each run's "lost" (avail, flushed).
#   LIB    muimaster.library (default /tmp/stab2/build/muimaster.library)
#   OUT    result folder (default $ZWORK/runkeep)
set -u
. "$(dirname "$0")/emu.sh"
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
REAL=${REAL:-$HOME/.cache/zune68k-real}
S=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$S/../../.." && pwd)
[ -d "$HOME/opt/amiga-gcc16/bin" ] && PATH=$HOME/opt/amiga-gcc16/bin:$PATH
NAME=$1; PORT=$2; WANT=$3; RUNS=${4:-3}
TARGET=zune; MACH=a1200-31
LIB=${LIB:-/tmp/stab2/build/muimaster.library}
OUT=${OUT:-$ZWORK/runkeep}; LOG=$OUT/$NAME.log
mkdir -p "$OUT" "$ZWORK/logs"
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
log() { echo "$(date +%T) $NAME: $*" | tee -a "$LOG" >&2; }
. "$TOP/tests/lean/real/programs.sh"
. "$S/suite-lib.sh"
MT=$ZWORK/memtally
[ -f "$MT" ] || ( cd "$S/../memtally" && m68k-amigaos-gcc -Os -m68000 -noixemul -s -o "$MT" memtally.c patch.s callseg.s ) || exit 1
ENTRY=
for e in "${PROGS[@]}"; do parse_prog "$e"; [ "$P_ID" = "$WANT" ] && { ENTRY=$e; break; }; done
[ -n "$ENTRY" ] || { echo "runkeep: no program $WANT" >&2; exit 2; }
PROGS=("$ENTRY"); parse_prog "$ENTRY"
amiga-test $MACH stop --as $NAME >/dev/null 2>&1
amiga-test $MACH reset --as $NAME >/dev/null
host_install "$(amiga-test $MACH dir --as $NAME)"
boot || { log "instance did not come up"; exit 1; }
wasabi run "makedir ENV:MUI" >/dev/null 2>&1
RD="$(sysvol)Real"; dir="$RD/$P_ID"; exe=${P_EXE%% *}; P_ARGS=; [ "$P_EXE" != "$exe" ] && P_ARGS=${P_EXE#* }
if [ -n "${RAMRUN[$P_ID]:-}" ]; then wasabi run "copy \"$dir\" RAM:$P_ID ALL QUIET" >/dev/null 2>&1; dir=RAM:$P_ID; fi
P_TASK="$dir/$exe"; P_TITLE=${TITLE[$P_ID]:-}
flushed() { wasabi run "avail flush" >/dev/null 2>&1; sleep 1; wasabi run "avail flush" >/dev/null 2>&1; avail; }
cycle() { hook pre; start_prog "$dir"; wait_window "$P_WAIT" && { settle 5; hook dismiss; }; quit_prog || log "did not quit"; sleep 2; }
cycle
wasabi put "$MT" RAM:memtally >/dev/null; wasabi run "protect RAM:memtally +e
run >NIL: <NIL: RAM:memtally lib muimaster.library" >/dev/null 2>&1; sleep 3
a=$(flushed)
wasabi run "RAM:memtally dump RAM:mt.txt" >/dev/null 2>&1; wasabi get RAM:mt.txt "$OUT/$WANT-0.mt" >/dev/null 2>&1
for k in $(seq 1 "$RUNS"); do
  cycle; b=$(flushed)
  wasabi run "RAM:memtally dump RAM:mt.txt" >/dev/null 2>&1; wasabi get RAM:mt.txt "$OUT/$WANT-$k.mt" >/dev/null 2>&1
  log "run $k: lost $((a - b))"; a=$b
done
python3 -I - "$OUT" "$WANT" "$RUNS" > "$OUT/$WANT-kept.txt" <<'EOF'
import sys
out, want, runs = sys.argv[1], sys.argv[2], int(sys.argv[3])
def blocks(f):
    b = {}
    for l in open(f, errors='replace'):
        p = l.split()
        if len(p) == 7 and all(c in '0123456789abcdefABCDEF' for c in p[0]):
            b[p[6]] = l
    return b
for k in range(1, runs + 1):
    a, b = blocks('%s/%s-%d.mt' % (out, want, k - 1)), blocks('%s/%s-%d.mt' % (out, want, k))
    new = [b[x] for x in b if x not in a]
    print('run %d: %d new blocks, %d bytes' % (k, len(new), sum(int(l.split()[0], 16) for l in new)))
    for l in sorted(new, key=lambda l: -int(l.split()[0], 16))[:40]:
        print('  ' + l.rstrip())
EOF
cat "$OUT/$WANT-kept.txt" | head -60
[ -n "${KEEP:-}" ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1
