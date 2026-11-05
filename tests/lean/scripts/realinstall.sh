#!/usr/bin/env bash
# realinstall.sh PORT: install the third-party classes (LIBS:mui/) and the real programs
# ($REALDIR/); MCCDIR= another class folder (default LIBS:mui) from ~/.cache/zune68k-real onto the instance at PORT.
set -u
# REALDIR: the drawer on the test machine that holds the programs, e.g. a
# volume of the test instance followed by "Real"; no default (machines differ)
REALDIR=${REALDIR:?set REALDIR to a drawer on the test machine}
export WASABI_HOST=127.0.0.1 WASABI_PORT=$1
R=${REAL:-$HOME/.cache/zune68k-real}
D=$(cd "$(dirname "$0")" && pwd)
M=${MCCDIR:-LIBS:mui}
wasabi run "makedir $M" >/dev/null 2>&1
for f in $R/nlist/x/MCC_NList/Libs/MUI/AmigaOS3/* $R/texteditor/x/MCC_TextEditor/Libs/MUI/AmigaOS3/* \
         $R/betterstring/x/MCC_BetterString/Libs/MUI/AmigaOS3/* $R/thebar/x/MCC_TheBar/Libs/MUI/AmigaOS3/*; do
  wasabi put "$f" "$M/$(basename "$f")" --force >/dev/null || echo "failed $f"
done
wasabi mkdir $REALDIR >/dev/null 2>&1
for f in $R/nlist/x/MCC_NList/Demos/AmigaOS3/*-Demo $R/texteditor/x/MCC_TextEditor/Demos/TextEditor-Demo-AmigaOS3 \
         $R/thebar/x/MCC_TheBar/Demos/demo*-AmigaOS3; do
  wasabi put "$f" "$REALDIR/$(basename "$f")" >/dev/null && wasabi run "protect $REALDIR/$(basename "$f") +e" >/dev/null
done
