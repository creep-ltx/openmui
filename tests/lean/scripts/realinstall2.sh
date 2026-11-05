#!/usr/bin/env bash
# realinstall2.sh PORT: the real programs (YAM 2.5, MUIMine, InstallerNG, AmFTP, AmiTradeCenter)
# into $REALDIR/<name>/ and codesets.library into LIBS: on the instance at PORT.
set -u
# REALDIR: the drawer on the test machine that holds the programs, e.g. a
# volume of the test instance followed by "Real"; no default (machines differ)
REALDIR=${REALDIR:?set REALDIR to a drawer on the test machine}
export WASABI_HOST=127.0.0.1 WASABI_PORT=$1
R=${REAL:-$HOME/.cache/zune68k-real}
D=$(cd "$(dirname "$0")" && pwd)
wasabi put $R/codesets/x/codesets/Libs/AmigaOS3/codesets.library LIBS:codesets.library --force >/dev/null
"$D/putdir.sh" "$R/yam25/x/YAM 2.5" $REALDIR/YAM
wasabi mkdir $REALDIR/MUIMine >/dev/null 2>&1
wasabi put $R/muimine/x/MUIMineInstall/progfiles/MUIMine $REALDIR/MUIMine/MUIMine >/dev/null
wasabi put $R/muimine/x/MUIMineInstall/progfiles/MUIMine.info $REALDIR/MUIMine/MUIMine.info >/dev/null
"$D/putdir.sh" $R/muimine/x/MUIMineInstall/images $REALDIR/MUIMine/images
"$D/putdir.sh" $R/installerng/x/InstallerNG $REALDIR/InstallerNG
"$D/putdir.sh" $R/amftp/x/AmFTP $REALDIR/AmFTP
"$D/putdir.sh" $R/atc/x/AmiTradeCenter $REALDIR/ATC
wasabi run "protect $REALDIR/#?/#? +e" >/dev/null 2>&1
wasabi run "protect $REALDIR/InstallerNG/c/#? +e" >/dev/null 2>&1
