#!/usr/bin/env bash
# realinstall2.sh PORT: the real programs (YAM 2.5, MUIMine, InstallerNG, AmFTP, AmiTradeCenter)
# into DH0:Real/<name>/ and codesets.library into LIBS: on the instance at PORT.
set -u
export WASABI_HOST=127.0.0.1 WASABI_PORT=$1
R=${REAL:-$HOME/.cache/zune68k-real}
D=$(cd "$(dirname "$0")" && pwd)
wasabi put $R/codesets/x/codesets/Libs/AmigaOS3/codesets.library LIBS:codesets.library --force >/dev/null
"$D/putdir.sh" "$R/yam25/x/YAM 2.5" DH0:Real/YAM
wasabi mkdir DH0:Real/MUIMine >/dev/null 2>&1
wasabi put $R/muimine/x/MUIMineInstall/progfiles/MUIMine DH0:Real/MUIMine/MUIMine >/dev/null
wasabi put $R/muimine/x/MUIMineInstall/progfiles/MUIMine.info DH0:Real/MUIMine/MUIMine.info >/dev/null
"$D/putdir.sh" $R/muimine/x/MUIMineInstall/images DH0:Real/MUIMine/images
"$D/putdir.sh" $R/installerng/x/InstallerNG DH0:Real/InstallerNG
"$D/putdir.sh" $R/amftp/x/AmFTP DH0:Real/AmFTP
"$D/putdir.sh" $R/atc/x/AmiTradeCenter DH0:Real/ATC
wasabi run "protect DH0:Real/#?/#? +e" >/dev/null 2>&1
wasabi run "protect DH0:Real/InstallerNG/c/#? +e" >/dev/null 2>&1
