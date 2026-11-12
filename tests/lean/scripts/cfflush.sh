#!/usr/bin/env bash
# cfflush.sh NAME PORT [LIB]: the regression test for stability round 2's class-file reset. The Pages
# demo under failn with its first counted LoadSeg failed (MASK 8000: LoadSeg only) after a flush:
# the private load of LIBS:OpenMUI/Register.mui fails, MUI_NewObjectA tries the class by name as an
# external class and OpenLibrary's the same file, which stays in the library list; then
# "avail flush" expunges it. Before the fix cf_expunge ran through a NULL SysBase and the machine
# reset. Prints "ok: no reset" or "FAIL: reset". Uses failsweep.sh to boot the instance (kept).
set -u
S=$(cd "$(dirname "$0")" && pwd)
. "$S/emu.sh"
NAME=$1; PORT=$2
export ZWORK=${ZWORK:-$HOME/.cache/zune68-lean} KEEP=1 LIB=${3:-${LIB:-/tmp/suite/build/muimaster.library}}
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
OUT=$ZWORK/cfflush-boot.tsv "$S/failsweep.sh" "$NAME" "$PORT" Pages 0 0 > "$ZWORK/cfflush-boot.log" 2>&1
wasabi run "avail flush" >/dev/null; wasabi run "avail flush" >/dev/null
wasabi run "stack 40000
run >NIL: RAM:failn run RAM:Prog 1 8000 >RAM:fn.txt" >/dev/null; sleep 6
wasabi arexx --timeout 5 PAGESDEMO.1 QUIT >/dev/null 2>&1; sleep 2
rep=$(wasabi run "type RAM:fn.txt" 2>/dev/null | tr -d "\r" | grep "failn: failed"); echo "${rep:-note: no call was failed (the class files stayed loaded), the test proves nothing this time}"
wasabi run "avail flush" >/dev/null 2>&1; sleep 4
if wasabi ping >/dev/null 2>&1 && wasabi run "list RAM:failn QUICK" >/dev/null 2>&1; then echo "ok: no reset"; r=0; else echo "FAIL: reset"; r=1; fi
amiga-test a1200-31 stop --as "$NAME" >/dev/null 2>&1
exit $r
