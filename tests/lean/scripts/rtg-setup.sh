#!/usr/bin/env bash
# rtg-setup.sh MACHINE NAME PORT P96DIR [WIDTH HEIGHT DEPTH]: a fresh copy of MACHINE (a1200-32-mui5
# is the one that boots with a 68030) with Picasso96 installed for the emulator's uaegfx board and
# the Workbench on an RTG screen (default 1280x1024, 24 bit), then booted with --cpu 030 --rtg.
# P96DIR is an unpacked Picasso96 3.6.3 (iComp) install folder (the user's own copy; 2.0 from the
# OS 3.9 CD does not work: ~/Projects/Knowledge/amiga/rtg-uaegfx-and-window-macros.md). The
# install is done by hand, as that note says: the three libraries, DEVS:Monitors/uaegfx with the
# tool type BOARDTYPE=uaegfx (set on the Amiga by tests/lean/settt.c, there is no OS tool for it),
# DEVS:Picasso96Settings = the .64 preset with its board name changed to uaegfx, and
# ENVARC:Sys/screenmode.prefs. The instance is left running (amiga-test MACHINE stop --as NAME).
# EMU=amiberry (default) or fsuae, as every lean script.
set -u
S=$(cd "$(dirname "$0")" && pwd)
. "$S/emu.sh"
[ -d "$HOME/opt/amiga-gcc16/bin" ] && PATH=$HOME/opt/amiga-gcc16/bin:$PATH
MACH=${1:?usage: rtg-setup.sh MACHINE NAME PORT P96DIR [W H DEPTH]}; NAME=$2; PORT=$3; P96=$4
W=${5:-1280}; H=${6:-1024}; DEPTH=${7:-24}
export WASABI_HOST=127.0.0.1 WASABI_PORT=$PORT
ZWORK=${ZWORK:-$HOME/.cache/zune68-lean}; mkdir -p "$ZWORK/logs"
fnd() { find "$P96" -ipath "*/$1" -print -quit; }   # the archive's folder names vary in case
amiga-test "$MACH" stop --as "$NAME" >/dev/null 2>&1
amiga-test "$MACH" reset --as "$NAME" >/dev/null || exit 1
D=$(amiga-test "$MACH" dir --as "$NAME")
mkdir -p "$D/Libs/Picasso96" "$D/Devs/Monitors" "$D/Prefs/Env-Archive/Sys"
for f in Libs/Picasso96/rtg.library Libs/Picasso96/emulation.library Libs/Picasso96API.library; do
  src=$(fnd "$f"); [ -n "$src" ] || { echo "rtg-setup: no $f in $P96" >&2; exit 1; }
  cp "$src" "$D/$f"
done
cp "$(fnd devs/monitors/Picasso96)" "$D/Devs/Monitors/uaegfx"
cp "$(fnd devs/monitors/Picasso96.info)" "$D/Devs/Monitors/uaegfx.info"
# the preset's BDNM chunk (32 bytes, the board's name) "none" -> "uaegfx"
python3 - "$(fnd devs/Picasso96Settings.64)" "$D/Devs/Picasso96Settings" <<'EOF'
import sys
b = bytearray(open(sys.argv[1], 'rb').read())
i = b.find(b'BDNM'); n = int.from_bytes(b[i+4:i+8], 'big')
b[i+8:i+8+n] = b'uaegfx'.ljust(n, b'\0')
open(sys.argv[2], 'wb').write(b)
EOF
# ENVARC:Sys/screenmode.prefs: IFF PREF, PRHD, SCRM (DisplayID, width, height, depth, control)
python3 - "$D/Prefs/Env-Archive/Sys/screenmode.prefs" "$W" "$H" "$DEPTH" <<'EOF'
import struct, sys
out, w, h, d = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
# uaegfx mode IDs: 0x50001000 + size index << 8 + depth code; 1280x1024 32-bit BGRA is 0x50071303
sizes = {(640,480): 1, (800,600): 2, (1024,768): 3, (1152,864): 4, (1200,900): 5, (1280,960): 6, (1280,1024): 7, (1600,1200): 8, (1920,1080): 9}
depths = {8: 0x1000, 15: 0x1100, 16: 0x1200, 24: 0x1303, 32: 0x1303}
did = 0x50000000 | (sizes[(w, h)] << 16) | depths[d]
prhd = struct.pack('>BBI', 0, 0, 0)
scrm = struct.pack('>4IIHHHH', 0, 0, 0, 0, did, w, h, d, 1)
def chunk(cid, b): return cid + struct.pack('>I', len(b)) + b + (b'\0' if len(b) & 1 else b'')
body = b'PREF' + chunk(b'PRHD', prhd) + chunk(b'SCRM', scrm)
open(out, 'wb').write(b'FORM' + struct.pack('>I', len(body)) + body)
print("screenmode.prefs: DisplayID %08x %dx%d depth %d" % (did, w, h, d))
EOF
# the tool type: on the Amiga, on the native screen, then reboot with the board
ST=$ZWORK/settt
[ -f "$ST" ] || m68k-amigaos-gcc -noixemul -Os -s -o "$ST" "$S/../settt.c" || exit 1
amiga-test "$MACH" start --as "$NAME" --port "$PORT" > "$ZWORK/logs/$NAME.log" 2>&1 < /dev/null || { tail -1 "$ZWORK/logs/$NAME.log"; exit 1; }
wasabi put "$ST" RAM:settt >/dev/null && wasabi run "protect RAM:settt +e
RAM:settt DEVS:Monitors/uaegfx BOARDTYPE=uaegfx"
amiga-test "$MACH" stop --as "$NAME"
amiga-test "$MACH" start --as "$NAME" --port "$PORT" --cpu 030 --rtg > "$ZWORK/logs/$NAME.log" 2>&1 < /dev/null || { tail -1 "$ZWORK/logs/$NAME.log"; exit 1; }
sleep 3
wasabi windows | head -1
