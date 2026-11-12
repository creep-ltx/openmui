#!/usr/bin/env bash
# rexxfuzz.sh NAME PORT CASES [PROG] [PORTNAME]: ARexx command fuzz on the Amiga (gate 2, robust
# input). Boots an a1200-31 instance NAME on PORT with the library LIB, starts PROG (default
# MUI-Demo from $ZWORK/demos; its ARexx port PORTNAME, default MUIDEMO.1) and sends it CASES
# commands made at random from MUI's built-ins (QUIT FORCE/S, HIDE, DEACTIVATE, SHOW, ACTIVATE,
# INFO ITEM/A, HELP FILE/A) with right, wrong, missing, too many, very long and binary arguments,
# unknown commands, empty lines, and commands of 1000 characters. QUIT is sent only at the end
# (its answer and the memory left are checked then). After each command the program must still
# answer (SHOW puts its window back after a HIDE). One line per command into OUT (default
# $ZWORK/rexxfuzz/NAME.tsv): n, the command (printable), the answer's rc, status (ok; hidden
# after a HIDE; nowindow, gone, dead are abnormal).
#   LIB (default /tmp/stab1/build/muimaster.library), SEED (1), FAST (8192), KEEP=1
set -u
. "$(dirname "$0")/emu.sh"
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}
S=$(cd "$(dirname "$0")" && pwd)
NAME=$1; PORT=$2; CASES=${3:-200}; PROG=${4:-MUI-Demo}; RP=${5:-MUIDEMO.1}
LIB=${LIB:-/tmp/stab1/build/muimaster.library}
FAST=${FAST:-8192}; SEED=${SEED:-1}
MACH=a1200-31
OUT=${OUT:-$ZWORK/rexxfuzz/$NAME.tsv}
mkdir -p "$(dirname "$OUT")" "$ZWORK/logs"
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
WASABI_BIN=$(type -P wasabi)
wasabi() { timeout 40 "$WASABI_BIN" "$@"; }
[ -f "$PROG" ] || PROG=$ZWORK/demos/$PROG
log() { echo "$(date +%T) $NAME: $*" >&2; }
boot() {
  amiga-test $MACH stop --as $NAME >/dev/null 2>&1
  ( amiga-test $MACH start --as $NAME --port $PORT --config fast_memory=$FAST --config fpu=68882 > "$ZWORK/logs/$NAME.log" 2>&1 < /dev/null & )
  local i
  for i in $(seq 1 60); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done
  for i in $(seq 1 30); do wasabi windows 2>/dev/null | grep -q '^ *window "Workbench"' && break; sleep 1; done
  sleep 2
}
avail() { wasabi run "avail flush" >/dev/null; sleep 1; wasabi run "avail flush" >/dev/null; sleep 1; wasabi run avail 2>/dev/null | awk '/^total/{print $3}'; }
start() {
  wasabi run "stack 40000
run >NIL: RAM:Prog" >/dev/null 2>&1
  local i; for i in $(seq 1 60); do [ "$(wasabi windows 2>/dev/null | grep -c 'task RAM:Prog')" -ge 1 ] && break; sleep 0.3; done
  sleep 2
}
boot; "$S/putlib.sh" "$LIB" >/dev/null; boot
wasabi put "$PROG" RAM:Prog >/dev/null; wasabi run "protect RAM:Prog +e" >/dev/null
A0=$(avail)
start
: > "$OUT"
python3 -I - "$SEED" "$CASES" > "$ZWORK/rexxfuzz/$NAME.cmds" <<'EOF'
import random, sys
rnd = random.Random(int(sys.argv[1])); n = int(sys.argv[2])
names = ['HIDE', 'SHOW', 'DEACTIVATE', 'ACTIVATE', 'INFO', 'HELP', 'quit', 'Quit', 'QUITX', 'XQUIT', 'INFOX', 'ShowMe', '', ' ', 'HELPME']
items = ['base', 'TITLE', 'author', 'copyright', 'Description', 'version', 'screen', 'nothing', '', '"', 'a b c', '*']
junk = 'abcXYZ019 ,="/\\*?!$%&()[]{}<>|;:.-_'
out = []
for i in range(n):
    r = rnd.random()
    if r < 0.15: c = rnd.choice(['HIDE', 'SHOW', 'DEACTIVATE', 'ACTIVATE'])
    elif r < 0.3: c = 'INFO ' + rnd.choice(items)
    elif r < 0.4: c = 'HELP ' + rnd.choice(['RAM:help.txt', 'RAM:', 'NIL:', 'T:x', '', 'RAM:help.txt extra', '"RAM:a b"', '/', 'bad:name'])
    elif r < 0.5: c = rnd.choice(names) + ' ' + ''.join(rnd.choice(junk) for _ in range(rnd.randint(0, 30)))
    elif r < 0.6: c = ''.join(rnd.choice(junk) for _ in range(rnd.randint(1, 40)))
    elif r < 0.7: c = rnd.choice(['INFO', 'HELP']) + ' ' + ''.join(rnd.choice('ab') for _ in range(rnd.choice([100, 300, 1000])))
    elif r < 0.8: c = ''.join(chr(rnd.randrange(32, 255)) for _ in range(rnd.randint(1, 60)))
    elif r < 0.9: c = rnd.choice(['INFO', 'HELP', 'SHOW']) + ' ' + ' '.join(rnd.choice(items) for _ in range(rnd.randint(2, 12)))
    else: c = 'INFO ' + rnd.choice(['=', 'ITEM=title', 'ITEM', '?', 'title title', ',,,', '\t', '"title"'])
    c = c.replace('\n', ' ')
    if c.upper().startswith('QUIT'): c = 'QUIT' + 'X' + c[4:]
    out.append(c)
    # a SHOW after every HIDE so the window is there to check
    if c.upper().startswith('HIDE'): out.append('SHOW')
sys.stdout.write('\n'.join(out) + '\n')
EOF
n=0; bad=0
while IFS= read -r c; do
  n=$((n + 1))
  ans=$(wasabi arexx --json --timeout 20 "$RP" "$c" 2>&1 | tr -d '\n' | head -c 300)
  rc=$(echo "$ans" | python3 -I -c 'import json,sys
try:
    d=json.loads(sys.stdin.read()); print(d.get("rc", d.get("result", "?")))
except Exception: print("?")')
  st=ok
  wasabi ping >/dev/null 2>&1 || st=dead
  [ "$st" = ok ] && ! wasabi ps 2>/dev/null | grep -q 'RAM:Prog' && st=gone
  [ "$st" = ok ] && [ "$(wasabi windows 2>/dev/null | grep -c 'task RAM:Prog')" = 0 ] && st=nowindow
  case "$c" in [Hh][Ii][Dd][Ee]*) [ "$st" = nowindow ] && st=hidden ;; esac   # HIDE's own doing; SHOW follows
  [ "$st" = ok ] || bad=$((bad + 1))
  printf '%s\t%s\t%s\t%s\n' "$n" "$(printf '%s' "$c" | tr -c '[:print:]' '.' | head -c 80)" "$rc" "$st" >> "$OUT"
  [ "$st" = dead ] && { log "command $n killed the machine: $c"; break; }
  [ "$st" = gone ] && { log "command $n ended the program: $c"; start; }
  [ "$st" = nowindow ] && { log "command $n lost the window: $c"; wasabi arexx "$RP" SHOW >/dev/null 2>&1; }
done < "$ZWORK/rexxfuzz/$NAME.cmds"
q=$(wasabi arexx --json "$RP" QUIT 2>&1 | tr -d '\n' | head -c 200); sleep 3
left=$(wasabi ps 2>/dev/null | grep -c 'RAM:Prog')
A1=$(avail)
log "$n commands, $bad abnormal; QUIT: $q; still running $left; lost $((A0 - A1)) bytes (negative: kept)"
echo "summary	$n	$bad	$left	$((A0 - A1))" >> "$OUT"
[ -n "${KEEP:-}" ] || amiga-test $MACH stop --as $NAME >/dev/null 2>&1
