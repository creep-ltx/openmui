#!/usr/bin/env bash
# suite-fetch.sh [NAME...]: download the real-program suite's archives (FETCH in
# tests/lean/real/programs.sh) into $REAL/<name>/arc/ (default ~/.cache/zune68k-real) and unpack
# each into its own empty folder $REAL/<name>/x/ with LHa for UNIX (~/opt/lha/bin/lha, or lha on
# PATH). The archives are untrusted data: nothing in them is run on this computer, and nothing is
# committed. Already downloaded archives are kept.
set -u
REAL=${REAL:-$HOME/.cache/zune68k-real}
S=$(cd "$(dirname "$0")" && pwd)
. "$S/../real/programs.sh"
LHA=$HOME/opt/lha/bin/lha; [ -x "$LHA" ] || LHA=lha
for e in "${FETCH[@]}"; do
  n=${e%% *}; u=${e#* }
  [ $# -gt 0 ] && ! printf '%s\n' "$@" | grep -qx "$n" && continue
  d=$REAL/$n; f=$d/arc/$(basename "${u%%\?*}" | sed 's/%2B/+/g')
  mkdir -p "$d/arc"
  if [ ! -s "$f" ]; then
    curl -sfL -m 600 -o "$f" "$u" || { echo "$n: download failed ($u)"; rm -f "$f"; continue; }
  fi
  if [ ! -d "$d/x" ] || [ -z "$(ls -A "$d/x" 2>/dev/null)" ]; then
    mkdir -p "$d/x"
    case "$f" in
      *.lha|*.lzh) ( cd "$d/x" && "$LHA" -xqf "$f" ) ;;
      *.zip) unzip -qo "$f" -d "$d/x" ;;
    esac
  fi
  echo "$n: $(stat -c %s "$f") bytes, $(find "$d/x" -type f | wc -l) files"
done
