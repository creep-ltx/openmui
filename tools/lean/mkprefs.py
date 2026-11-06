#!/usr/bin/env python3
# mkprefs.py OUT ID=VALUE...: a Zune settings file (FORM PREF, PRHD, MUIC) with ULONG items,
# e.g. mkprefs.py global.prefs 0x50a=15 (MUICFG_Window_Buttons: all four title gadgets) for
# ENV:Zune/global.prefs
import struct, sys
items = b''
for a in sys.argv[2:]:
    k, v = a.split('=')
    items += struct.pack('>III', int(k, 0), 4, int(v, 0))
prhd = b'PRHD' + struct.pack('>I', 6) + bytes(6)
muic = b'MUIC' + struct.pack('>I', len(items)) + items
body = b'PREF' + prhd + muic
open(sys.argv[1], 'wb').write(b'FORM' + struct.pack('>I', len(body)) + body)
