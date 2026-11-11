# suite-lib.sh: functions of suite.sh (sourced; uses TARGET MACH NAME PORT LIB REAL OUT LOG RD LEAK)

# ---- the host side: copy into the instance's drive folder before it boots ----
# a folder of the drive, found case-insensitively (FS-UAE's host folders are case-sensitive on
# the host, the Amiga side is not: "MUI/Libs/mui" and "MUI/Libs/MUI" would be two folders)
hostdir() { # ROOT PATH: print ROOT/PATH with the case found on disk, made where missing
  local d=$1 part p; local IFS=/
  for part in $2; do
    p=$(find "$d" -maxdepth 1 -mindepth 1 -type d -iname "$part" -print -quit)
    [ -n "$p" ] || { p="$d/$part"; mkdir -p "$p"; }
    d=$p
  done
  echo "$d"
}
hostfile_exists() { [ -n "$(find "$1" -maxdepth 1 -iname "$2" -print -quit 2>/dev/null)" ]; }

host_install() { # DRIVE
  local D=$1 cdir ldir e src dst n=0 k=0 p
  ldir=$(hostdir "$D" Libs)
  if [ "$TARGET" = zune ]; then
    cdir=$(hostdir "$D" Libs/MUI)
    cp "$LIB" "$ldir/muimaster.library"
    if [ -d "$(dirname "$LIB")/Zune" ]; then
      mkdir -p "$ldir/Zune"; cp "$(dirname "$LIB")"/Zune/*.mui "$ldir/Zune/"
    fi
  else
    cdir=$(hostdir "$D" MUI/Libs/mui)
  fi
  local own=$(dirname "$LIB")/Libs/MUI z=0
  for e in "${CLASSES[@]}"; do
    src=${e%% *}; dst=$(basename "$src"); [ "$e" != "$src" ] && dst=${e#* }
    [ -f "$REAL/$src" ] || { log "missing class $REAL/$src"; continue; }
    if [ "$TARGET" != zune ] && hostfile_exists "$cdir" "$dst"; then k=$((k+1)); continue; fi
    # classes that come with MUI 3.8 itself (Busy, Listtree): on Zune its own build of them
    # ("make plugins", Libs/MUI next to the library), as Zune's installer puts them; MUI 3.8's
    # own Listtree.mcc reset the machine under Zune (2026-11-05)
    if [ "$TARGET" = zune ] && [ "${src#mui38usr/}" != "$src" ]; then
      if [ -f "$own/$dst" ]; then cp "$own/$dst" "$cdir/$dst"; z=$((z+1)); continue; fi
      case "$dst" in *.mcp) continue ;; esac
    fi
    cp "$REAL/$src" "$cdir/$dst"; n=$((n+1))
  done
  if [ "$TARGET" = zune ] && [ -d "$own" ]; then
    # Zune's other own classes (Aboutbox, Calltips, Clock, Rawimage...: what MUI 4/5 bring
    # themselves) where no third-party file of that name is installed, as its installer does
    for e in "$own"/*.mcc "$own"/*.mcp; do
      hostfile_exists "$cdir" "$(basename "$e")" && continue
      cp "$e" "$cdir/"; z=$((z+1))
    done
    [ -f "$own/Pixmap.mui" ] && mkdir -p "$ldir/Zune" && cp "$own/Pixmap.mui" "$ldir/Zune/"
    log "classes: $z of Zune's own ($own; MUI 3.8's Busy and Listtree replaced)"
  fi
  log "classes: $n copied to ${cdir#$D/}, $k kept (the MUI install has them)"
  for e in "${LIBRARIES[@]}"; do
    dst=$(basename "$e")
    hostfile_exists "$ldir" "$dst" && continue
    [ "$TARGET" != zune ] && [ -d "$D/MUI/Libs" ] && hostfile_exists "$D/MUI/Libs" "$dst" && continue
    cp "$REAL/$e" "$ldir/$dst"
  done
  mkdir -p "$(hostdir "$D" Prefs/Env-Archive/MUI)"
  # the menu helper (tests/lean/menupos.c)
  local mp=$ZWORK/suite-real/menupos
  if [ ! -f "$mp" ] || [ "$TOP/tests/lean/menupos.c" -nt "$mp" ]; then
    m68k-amigaos-gcc -m68000 -noixemul -Os -s -o "$mp" "$TOP/tests/lean/menupos.c" || log "menupos not built (m68k-amigaos-gcc on PATH?)"
  fi
  [ -f "$mp" ] && cp "$mp" "$(hostdir "$D" C)/menupos"
  # the programs
  mkdir -p "$D/Real"
  for e in "${PROGS[@]}"; do
    parse_prog "$e"
    [ -d "$REAL/$P_SRC" ] || { log "missing program folder $REAL/$P_SRC"; continue; }
    mkdir -p "$D/Real/$P_ID"; cp -a "$REAL/$P_SRC/." "$D/Real/$P_ID/"
    p=pre_host_${P_ID//-/_}; declare -F "$p" >/dev/null && "$p" "$D/Real/$P_ID" "$D"
  done
}

parse_prog() { # the table line ID|GROUP|SOURCE|EXE|WAIT|EXTRAS
  IFS='|' read -r P_ID P_GROUP P_SRC P_EXE P_WAIT P_EXTRAS <<<"$1"
}

# ---- the instance ----
boot() {
  amiga-test $MACH start --as $NAME --port $PORT --config fast_memory=8192 --config fpu=68882 \
    > "$ZWORK/logs/$NAME.log" 2>&1 < /dev/null &
  local i
  for i in $(seq 1 60); do sleep 1; wasabi ping >/dev/null 2>&1 && break; done
  for i in $(seq 1 30); do wasabi windows 2>/dev/null | grep -q '^ *window "Workbench"' && break; sleep 1; done
  sleep 2; wasabi ping >/dev/null 2>&1
}
reboot_instance() { amiga-test $MACH stop --as $NAME >/dev/null 2>&1; boot; wasabi run "makedir ENV:MUI" >/dev/null 2>&1; }
sysvol() { wasabi run assign 2>/dev/null | awk '$1=="SYS"{print $2; exit}'; }

# ---- the program's windows ----
# wasabi windows --raw: "W addr x y w h ... flags task title" (screen pixels, rows not doubled).
# A program's windows are those of its task (the command as started) or, when the table gives
# TITLE[id] (programs that start a process of their own and let the command end), those whose
# title matches that regular expression.
P_TASK=; P_TITLE=
wins() { # one line per window of the program: "title<TAB>x,y wxh<TAB>address"
  wasabi windows --raw 2>/dev/null | awk -F'\t' -v t="$P_TASK" -v ti="$P_TITLE" '
    $1=="W" && ($(NF-1)==t || (ti!="" && $(NF-1)!="Workbench" && $NF ~ ti)) {print $NF "\t" $3 "," $4 " " $5 "x" $6 "\t" $2}'
}
winline() { # TITLE-PART: the raw line of the program's first window whose title contains it
  wasabi windows --raw 2>/dev/null | awk -F'\t' -v t="$P_TASK" -v ti="$P_TITLE" -v p="$1" '
    $1=="W" && ($(NF-1)==t || (ti!="" && $(NF-1)!="Workbench" && $NF ~ ti)) && (p=="" || index($NF,p)) {print; exit}'
}
running() { # "wasabi ps" cuts a long task name (Workbench:Real/simplemail/SimpleMa): a prefix
  wasabi ps 2>/dev/null | grep -qF "${P_TASK:0:30}" && return 0
  [ -n "$P_TITLE" ] && [ -n "$(wins)" ]
}
# free memory, without "avail flush": on the MUI 3.8 machine a flush (even on a fresh boot,
# before any MUI program) leaves the next MUI program without a window, waiting for ever
# (NList-Demo, then MUI's own Class1 demo too). The second run measured between two of these
# reuses the libraries and classes the first run loaded, so the difference is what the run
# itself keeps.
avail() { sleep 2; wasabi run avail 2>/dev/null | awk '/^total/{print $3}'; }
grab() { local f; f=$(wasabi grab 2>/dev/null | tail -1 | sed 's/.*-> //; s/ (.*//'); [ -f "$f" ] && cp "$f" "$1"; }
alerts() { wasabi info 2>/dev/null | grep -i -c -e 'guru' -e 'alert [0-9a-f]'; }
yscale() { local sh; sh=$(wasabi windows --raw 2>/dev/null | awk -F'\t' '$1=="S"{print $6; exit}'); [ "${sh:-256}" -le 300 ] && echo 2 || echo 1; }

# ---- helpers for the hooks in programs.sh ----
key() { wasabi key "$@" >/dev/null 2>&1; }
titlepoint() { # ADDRESS: "x y" (screen pixels) on the window's title bar that no window in front
               # of it covers ("wasabi windows" lists the front one first), or its middle
  wasabi windows --raw 2>/dev/null | awk -F'\t' -v a="$1" '
    $1=="S" { s++ }
    $1=="W" { n++; S[n]=s; X[n]=$3; Y[n]=$4; W[n]=$5; H[n]=$6; if ($2==a) t=n }
    END { if (!t) exit; y = Y[t] + 4
      for (x = X[t] + 30; x < X[t] + W[t] - 60; x += 8) {
        ok = 1; for (i = 1; i < t; i++) if (S[i]==S[t] && x >= X[i] && x < X[i]+W[i] && y >= Y[i] && y < Y[i]+H[i]) ok = 0
        if (ok) { print x, y; exit } }
      print X[t] + int(W[t]/2), y }'
}
activate_addr() { # ADDRESS: make that window the active one (a click on its title bar)
  local p ys; p=$(titlepoint "$1"); [ -n "$p" ] || return 1; ys=$(yscale)
  set -- $p; wasabi mouse click $1 $(( $2 * ys )) >/dev/null; sleep 0.5
}
activate() { # TITLE-PART: activate the program's window whose title contains it
  local l; l=$(winline "$1"); [ -n "$l" ] || return 1
  activate_addr "$(echo "$l" | cut -f2)"
}
clickrel() { # TITLE-PART DX DY: click in the program's window whose title contains TITLE-PART, at
             # DX,DY from its left/top edge (negative: from the right/bottom edge), in grab space
  local l x y w h ys cx cy; l=$(winline "$1"); [ -n "$l" ] || return 1; ys=$(yscale)
  x=$(echo "$l" | cut -f3); y=$(echo "$l" | cut -f4); w=$(echo "$l" | cut -f5); h=$(echo "$l" | cut -f6)
  if [ "$2" -lt 0 ]; then cx=$((x + w + $2)); else cx=$((x + $2)); fi
  if [ "$3" -lt 0 ]; then cy=$(( (y + h) * ys + $3 )); else cy=$(( y * ys + $3 )); fi
  wasabi mouse click $cx $cy >/dev/null; sleep 1.5
}
press() { # TITLE-PART KEY...: keys to that window (MUI 3.8 buttons cannot be clicked by name;
          # a MUI button answers its underlined letter)
  local t=$1; shift; activate "$t" || return 1; wasabi key press "$@" >/dev/null 2>&1; sleep 1.5
}
waitwin() { # TITLE-PART [SECONDS]: wait until the program has a window whose title contains it
  local i; for i in $(seq 1 $(( ${2:-20} * 2 ))); do [ -n "$(winline "$1")" ] && return 0; sleep 0.5; done; return 1; }
# pick a menu item with the right mouse button: MENU/ITEM[/SUB] of the program's first window (or
# the one whose title contains WINDOWTITLE); "wasabi ui menu" sends a message that MUI's windows
# do not act on. Positions come from the window's menu strip (C:menupos, tests/lean/menupos.c),
# in screen pixels; the mouse is in grab space (rows doubled on a 256-line screen).
menu() { # PATH [WINDOWTITLE]
  local path=$1 line addr x y w pos= ys m i s
  IFS=/ read -r m i s <<<"$path"
  # the first of the program's windows whose menu strip has the item (not every window has one)
  while IFS= read -r line; do
    addr=$(echo "$line" | cut -f2)
    pos=$(wasabi run "C:menupos ${addr#0x} \"$m\" \"$i\" ${s:+\"$s\"}" 2>/dev/null | tail -1)
    # menupos answers "none" (no such item) or "failed" (no menu strip read:
    # under load the window may be gone); only numbers are a position
    case $pos in [0-9]*) break ;; esac
    pos=
  done < <(wasabi windows --raw 2>/dev/null | awk -F'\t' -v t="$P_TASK" -v ti="$P_TITLE" -v p="${2:-}" '
    $1=="W" && ($(NF-1)==t || (ti!="" && $(NF-1)!="Workbench" && $NF ~ ti)) && (p=="" || index($NF,p))')
  [ -n "$pos" ] || return 1
  x=$(echo "$line" | cut -f3); y=$(echo "$line" | cut -f4); w=$(echo "$line" | cut -f5)
  ys=$(yscale)
  set -- $pos
  activate_addr "$addr"
  wasabi mouse move $1 $(( $2 * ys )) >/dev/null
  wasabi mouse down --right >/dev/null; sleep 0.3
  wasabi mouse move $3 $(( $4 * ys )) >/dev/null; sleep 0.3
  if [ $# -ge 6 ]; then wasabi mouse move $5 $(( $6 * ys )) >/dev/null; sleep 0.3; fi
  wasabi mouse up --right >/dev/null
}
put_text() { # REMOTE TEXT: write a small file on the Amiga (printf %b escapes)
  local t; t=$(mktemp); printf '%b' "$2" > "$t"; wasabi put "$t" "$1" --force >/dev/null; rm -f "$t"; }
answer() { # LABEL...: click the first matching button of each system requester (EasyRequest) of
           # the program (Intuition gadgets: "wasabi ui click" finds them by label)
  local l w
  while IFS= read -r w; do
    [ -n "$w" ] || continue
    for l in "$@"; do wasabi ui click "$l" --window "$w" >/dev/null 2>&1 && { sleep 1; break; }; done
  done < <(wins | cut -f1)
}

# ---- one program ----
start_prog() { # DIR
  wasabi run "cd \"$1\"
stack 65536
run >NIL: <NIL: $P_TASK $P_ARGS" >/dev/null 2>&1
}
wait_window() { # SECONDS: 0 when a window of the program is open
  local end=$((SECONDS + $1))      # wall clock: wasabid answers slowly when a program hogs the CPU
  while [ $SECONDS -lt $end ]; do
    [ -n "$(wins)" ] && return 0
    running || { sleep 2; [ -n "$(wins)" ] && return 0; running || return 1; }
    sleep 0.5
  done
  return 1
}
gone() { local end=$((SECONDS + ${1:-15})); while [ $SECONDS -lt $end ]; do running || return 0; sleep 1; done; return 1; }
# a "really quit?" requester that the last action opened: system requesters (EasyRequest) by
# their button label, MUI ones by the underlined letter of the usual yes-button (o for Okay/Ok,
# y for Yes, q for Quit), each key only while that window is still open. MUI 3.8's buttons could
# be clicked by label too, Zune's cannot ("wasabi ui list" sees no objects in Zune's windows), so
# both are answered by keys alone.
confirm() { # ADDRESSES-BEFORE
  local w t k
  answer Quit Yes Ok OK
  while IFS=$'\t' read -r t _ w; do
    [ -n "$w" ] || continue
    printf '%s\n' "$1" | grep -qx "$w" && continue
    for k in o y q; do
      wins | cut -f3 | grep -qx "$w" || break
      activate "$t" && key press "$k"; sleep 2
    done
  done < <(wins)
}
close_all() { # the close gadget of each window, front one first
  local k w before end=$((SECONDS + 90))
  for k in 1 2 3 4 5 6 7 8; do
    [ $SECONDS -lt $end ] || break
    w=$(wins | head -1 | cut -f1)
    [ -z "$w" ] && break
    before=$(wins | cut -f3)
    wasabi ui click close --window "$w" >/dev/null 2>&1 || { activate "$w"; key press esc; }
    sleep 3
    running && confirm "$before"
  done
  gone 15
}
# quitting: the program's own quit hook, else its quit menu item (QUITMENU[id], default
# Project/Quit), then the close gadgets, then Ctrl-C. 0 when it ended.
quit_prog() {
  local h=quit_${P_ID//-/_} before
  if declare -F "$h" >/dev/null; then "$h"; gone 5 && return 0; fi
  before=$(wins | cut -f3)
  if [ "${QUITMENU[$P_ID]:-Project/Quit}" != - ] && menu "${QUITMENU[$P_ID]:-Project/Quit}"; then
    sleep 3; running && confirm "$before"
    gone 10 && return 0
  fi
  close_all && return 0
  wasabi kill "$P_TASK" >/dev/null 2>&1; sleep 5
  ! running
}
settle() { wasabi wait still --quiet 2 --timeout ${1:-15} >/dev/null 2>&1; }
# every wasabi call ends within 40 s, also when the machine froze
WASABI_BIN=$(type -P wasabi)
wasabi() { timeout 40 "$WASABI_BIN" "$@"; }
hook() { local h=${1}_${P_ID//-/_}; declare -F "$h" >/dev/null || return 0; "$h"; settle 10; }
alive() { wasabi ping >/dev/null 2>&1 || { sleep 3; wasabi ping >/dev/null 2>&1; }; }
# why wasabid stopped answering: "reset" (the machine rebooted by itself: wasabid answers again
# within 45 s, started by the startup sequence), "frozen" (it does not)
gone_why() { local i; for i in $(seq 1 15); do sleep 3; wasabi ping >/dev/null 2>&1 && { echo reset; return; }; done; echo frozen; }
# sixth column: the memory the second run holds with its windows open (bytes less free than
# before it started; libraries and classes are in memory from the first run, so this is the
# program's own memory, its objects and windows), "?" without a second run; seventh: the same for
# the first run, from a freshly booted machine: what the program costs in all (the library, its
# classes, and what MUI keeps after the program quits, which a second run then reuses)
result() { printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$P_ID" "$1" "$2" "$3" "$4" "${5:-?}" "${inuse1:-?}" | tee -a "$OUT/results.tsv" >&2; }

# --tally: the run TALLY_RUN (1 or 2) goes under memtally runall (every task's AllocMem/FreeMem),
# its live blocks with the windows open are written to $OUT/<id>.mt (run 2) or <id>-1.mt (run 1).
# TALLY_RUN=keep: "memtally lib muimaster.library" from before the first run (every task, also
# the library's own load), dumped with the windows open (<id>-1.mt) and after the program quit
# (<id>-q.mt: what stays in memory after it, code and whatever MUI keeps for later runs)
tally_start() { # RUN: copy memtally next to the program (its PROGDIR: is memtally's) and set mt
  mt=; mtq=; [ -n "${TALLY:-}" ] || return 0
  if [ "${TALLY_RUN:-2}" = keep ] && [ "$1" = 1 ]; then
    mtq=RAM:memtally
    wasabi put "$TALLY" "$mtq" >/dev/null && wasabi run "protect $mtq +e
run >NIL: <NIL: $mtq lib muimaster.library" >/dev/null
    sleep 3; return 0
  fi
  [ "${TALLY_RUN:-2}" = "$1" ] || return 0
  mt="$(dirname "$P_TASK")/memtally"
  wasabi put "$TALLY" "$mt" >/dev/null && wasabi run "protect \"$mt\" +e" >/dev/null
}
tally_run() { # the program in memtally's process; its windows then belong to memtally's command
  wasabi run "cd \"$(dirname "$P_TASK")\"
stack 65536
run >NIL: <NIL: \"$mt\" runall \"$P_TASK\"" >/dev/null 2>&1
  task0=$P_TASK; P_TASK=$mt
}
tally_dump() { # NAME
  local t=${mt:-$mtq}
  [ -n "$t" ] || return 0
  wasabi run "\"$t\" dump RAM:mt.txt" >/dev/null 2>&1 && wasabi get RAM:mt.txt "$OUT/$1.mt" >/dev/null 2>&1 \
    && wasabi run "delete RAM:mt.txt" >/dev/null 2>&1
}
tally_end() { [ -n "$mt" ] && P_TASK=$task0; mt=; }

run_prog() {
  local id=$P_ID dir="$RD/$P_ID" exe=${P_EXE%% *} wl= status=ok left=0 lost=? al0 al1 n x before w a0 a1 why inuse=? mt= mtq= task0=
  P_ARGS=; [ "$P_EXE" != "$exe" ] && P_ARGS=${P_EXE#* }
  if [ -n "${RAMRUN[$id]:-}" ]; then   # run from RAM: (before the first avail: the copy is memory)
    wasabi run "copy \"$dir\" RAM:$id ALL QUIET" >/dev/null 2>&1; dir=RAM:$id
  fi
  P_TASK="$dir/$exe"; P_TITLE=${TITLE[$id]:-}
  al0=$(alerts)
  log "$id: start $P_TASK"
  tally_start 1
  local f0 f1 inuse1=?
  f0=$(avail)
  hook pre
  if [ -n "$mt" ]; then tally_run; else start_prog "$dir"; fi
  if ! wait_window "$P_WAIT"; then
    status=nowindow
    if ! alive; then
      why=$(gone_why); log "$id: the machine is $why before a window opened, rebooting"
      result "$why" ? ? ""; reboot_instance; return
    fi
    if running; then
      # a program that never waits (state run/ready) hogs the CPU: a loop, not a slow start
      wasabi ps 2>/dev/null | grep -F "$P_TASK" | awk '{print $4}' | grep -q -e run -e ready && status="$status,busy"
    else status=exited; fi
  fi
  settle 10
  if ! alive; then
    why=$(gone_why); grab "$OUT/$id.png"; log "$id: the machine is $why after the first window, rebooting"
    result "$why" ? ? "$wl"; reboot_instance; return
  fi
  hook dismiss
  f1=$(avail); [ -n "$f0" ] && [ -n "$f1" ] && inuse1=$((f1-f0))
  tally_dump "$id-1"
  wl=$(wins | cut -f1,2 | tr '\t\n' '@;')
  [ -z "$wl" ] && [ "$status" = ok ] && status=nowindow
  grab "$OUT/$id.png"
  # extra windows from the menus
  n=0
  while IFS= read -r x; do
    [ -n "$x" ] && [ "$x" != - ] || continue
    n=$((n+1))
    before=$(wins | cut -f3)
    menu "$x" || log "$id: menu $x not found"
    # up to 40 s for a new window other than an error window (YAM's configuration window came
    # after ~15 s of "Error messages" on Zune), then settle
    local t0=$SECONDS
    while [ $((SECONDS - t0)) -lt 40 ] && [ -z "$(wins | grep -v -F -f <(echo "${before:-none}") | cut -f1 | grep -v -i error)" ]; do sleep 1; done
    sleep 2; settle 10
    wl="$wl|extra$n=$x:$(wins | grep -v -F -f <(echo "${before:-none}") | cut -f1,2 | tr '\t\n' '@;')"
    grab "$OUT/$id-extra$n.png"
    while IFS= read -r w; do
      [ -n "$w" ] || continue
      wasabi ui click close --window "$w" >/dev/null 2>&1 || { activate "$w"; key press esc; }; sleep 2
    done < <(wins | grep -v -F -f <(echo "${before:-none}") | cut -f1)
  done < <(echo "$P_EXTRAS" | tr ';' '\n')
  quit_prog || left=1
  tally_end
  [ -n "${mtq:-}" ] && { settle 5; tally_dump "$id-q"; }
  if ! alive; then
    # wasabid (on FS-UAE's emulated bsdsocket) dies when some network programs quit: "frozen"
    # then means wasabid is gone, not necessarily the machine
    why=$(gone_why); status="$status,$why-at-quit"; log "$id: $why at quit, rebooting"; reboot_instance; left=?; al0=$(alerts)
  fi
  if [ "$status" = ok ] && [ "$left" = 0 ] && [ "$LEAK" = 1 ]; then
    tally_start 2
    a0=$(avail)
    hook pre
    if [ -n "$mt" ]; then tally_run; else start_prog "$dir"; fi
    wait_window "$P_WAIT"; settle 10
    hook dismiss
    a1=$(avail); [ -n "$a0" ] && [ -n "$a1" ] && inuse=$((a1-a0))
    tally_dump "$id"
    if ! quit_prog; then lost="?(2nd run did not quit)"; left=2
    elif ! alive; then lost="?($(gone_why) at 2nd quit)"; reboot_instance; al0=$(alerts)
    else a1=$(avail); lost=$((a0-a1)); fi
    tally_end
  fi
  alive || { status="$status,$(gone_why)"; reboot_instance; al0=$(alerts); }
  al1=$(alerts)
  [ "${al1:-0}" -gt "${al0:-0}" ] && status="$status,alert"
  result "$status" "$left" "$lost" "$wl" "$inuse"
  # every program starts on a freshly booted machine: what one program leaves behind (a task
  # still running, memory it overwrote, MUI 3.8 after "avail flush": the next program's window
  # did not open) must not decide the next one's result
  reboot_instance
}
