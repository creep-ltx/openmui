#!/usr/bin/env bash
# suite.sh: the real-program suite. Boots its own emulator instance of one target (Amiberry
# headless by default, EMU=fsuae for FS-UAE: emu.sh; the same machine copies either way), installs the
# programs of tests/lean/real/programs.sh and their classes, then for each program: start, wait for
# its first window, answer start-up requesters, screenshot, open the listed extra windows
# (menus) with a screenshot each, quit; a second run measures the memory it loses.
#
#   suite.sh TARGET [options] [ID...]       (no ID: every program of the target's groups)
#   TARGET   zune  - a1200-31 (OS 3.1) with the library given by --lib (default /tmp/suite/build)
#            mui38 - a1200-31-mui38 (OS 3.1 + MUI 3.8)
#            mui5  - a1200-32-mui5 (OS 3.2.1 + MUI 5)
#   --lib FILE     muimaster.library to test (zune; its class files from Zune/ next to it)
#   --port N       wasabid port (default zune 1410, mui38 1411, mui5 1412)
#   --name NAME    instance name (default suite-TARGET)
#   --groups LIST  program groups, e.g. mui3,mui5 (default: zune and mui38 mui3, mui5 mui5)
#   --tag TAG      result folder $ZWORK/suite-real/TAG (default TARGET)
#   --no-leak      skip the second run (memory)
#   --behave-as ID Zune: MUI3.8, MUI4 or MUI5 for all programs (default: each
#                  program gets the MUI it asks for)
#   --tally FILE   run the second run under FILE (tests/lean/memtally built, "memtally runall":
#                  every task's AllocMem/FreeMem) and write its live blocks with the windows open
#                  to TAG/<id>.mt (read with tools/lean/mtlib.py); its in-use figure then
#                  includes memtally's own table. TALLY_RUN=1: the first run instead (from a
#                  fresh boot, the class files loaded by ramlib) to TAG/<id>-1.mt
#   --keep         leave the instance running at the end (default: stopped)
#   --reuse        do not make a fresh copy of the machine (files from the last run stay)
#   --update       keep the other programs' lines in results.tsv (run some programs again)
#   EMU=amiberry|fsuae in the environment: the emulator (default amiberry)
#
# Results: $ZWORK/suite-real/TAG/results.tsv (one line per program: id, status, windows with
# sizes, still running after quit, in-use before minus after the 2nd run (negative: memory
# kept), alert, memory in use with the 2nd run's windows open), TAG/<id>.png and
# TAG/<id>-extraN.png, TAG/log.txt. Compare two targets with suite-compare.py.
# Downloads: $REAL (default ~/.cache/zune68k-real/<name>/x/, see tests/lean/real-programs.md).
set -u
. "$(dirname "$0")/emu.sh"   # EMU=amiberry|fsuae: which emulator boots the machine
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
REAL=${REAL:-$HOME/.cache/zune68k-real}
S=$(cd "$(dirname "$0")" && pwd)
[ -d "$HOME/opt/amiga-gcc16/bin" ] && PATH=$HOME/opt/amiga-gcc16/bin:$PATH   # for menupos
TOP=$(cd "$S/../../.." && pwd)

TARGET=${1:?usage: suite.sh zune|mui38|mui5 [options] [ID...]}; shift
LIB=/tmp/suite/build/muimaster.library; PORT=; NAME=; PGROUPS=; TAG=; LEAK=1; BEHAVE=; KEEP=0; REUSE=0; UPDATE=0; TALLY=; ONLY=()
while [ $# -gt 0 ]; do
  case "$1" in
    --lib) LIB=$2; shift ;; --port) PORT=$2; shift ;; --name) NAME=$2; shift ;;
    --groups) PGROUPS=$2; shift ;; --tag) TAG=$2; shift ;; --no-leak) LEAK=0 ;;
    --behave-as) BEHAVE=$2; shift ;; --tally) TALLY=$2; shift ;;
    --keep) KEEP=1 ;; --reuse) REUSE=1 ;; --update) UPDATE=1 ;; -*) echo "suite.sh: unknown option $1" >&2; exit 2 ;;
    *) ONLY+=("$1") ;;
  esac; shift
done
case "$TARGET" in
  zune)  MACH=a1200-31;       PORT=${PORT:-1410}; PGROUPS=${PGROUPS:-mui3} ;;
  mui38) MACH=a1200-31-mui38; PORT=${PORT:-1411}; PGROUPS=${PGROUPS:-mui3} ;;
  mui5)  MACH=a1200-32-mui5;  PORT=${PORT:-1412}; PGROUPS=${PGROUPS:-mui5} ;;
  *) echo "suite.sh: target zune, mui38 or mui5" >&2; exit 2 ;;
esac
NAME=${NAME:-suite-$TARGET}; TAG=${TAG:-$TARGET}
OUT=$ZWORK/suite-real/$TAG; mkdir -p "$OUT" "$ZWORK/logs"
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
LOG=$OUT/log.txt
log() { echo "$(date +%T) $*" | tee -a "$LOG" >&2; }

. "$TOP/tests/lean/real/programs.sh"
. "$S/suite-lib.sh"

# ---- install on the host side (the instance's drive folder), before it boots ----
[ "$REUSE" = 1 ] || [ "$UPDATE" = 1 ] || : > "$LOG"
amiga-test $MACH stop --as $NAME >/dev/null 2>&1
if [ "$REUSE" = 0 ]; then
  amiga-test $MACH reset --as $NAME >/dev/null
  D=$(amiga-test $MACH dir --as $NAME)
  log "install into $D ($TARGET)"
  host_install "$D"
fi
boot || { log "instance did not come up"; exit 1; }
VOL=$(sysvol); RD="${VOL}Real"
log "SYS: is $VOL, programs in $RD"
wasabi run "makedir ENV:MUI" >/dev/null 2>&1
# Zune gives each program the MUI it asks for (Zune prefs "Behave as" Automatic);
# --behave-as MUI3.8|MUI4|MUI5 sets one for all programs (ENVARC: so that it
# survives the reboot before each program)
if [ "$TARGET" = zune ] && [ -n "$BEHAVE" ]; then
  wasabi run "makedir ENVARC:Zune
echo >ENVARC:Zune/BehaveAs $BEHAVE
makedir ENV:Zune
copy ENVARC:Zune/BehaveAs ENV:Zune/" >/dev/null 2>&1
  log "Zune behaves as $BEHAVE for all programs (ENVARC:Zune/BehaveAs)"
fi

# ---- run ----
if [ "$UPDATE" = 1 ] && [ -f "$OUT/results.tsv" ]; then
  # the lines of the programs run now go, the others stay (in their order)
  grep -v -F -f <(printf '%s\t\n' "${ONLY[@]}") "$OUT/results.tsv" > "$OUT/results.tsv.new"
  mv "$OUT/results.tsv.new" "$OUT/results.tsv"
else
  : > "$OUT/results.tsv"
fi
for e in "${PROGS[@]}"; do
  parse_prog "$e"
  [ ${#ONLY[@]} -gt 0 ] && ! printf '%s\n' "${ONLY[@]}" | grep -qx "$P_ID" && continue
  [ ${#ONLY[@]} -eq 0 ] && ! echo ",$PGROUPS," | grep -q ",$P_GROUP," && continue
  run_prog
done
wasabi info 2>/dev/null | grep -i -e guru -e alert >> "$LOG"
[ "$KEEP" = 1 ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1
log "done: $OUT/results.tsv"
cat "$OUT/results.tsv"
