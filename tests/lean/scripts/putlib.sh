#!/usr/bin/env bash
# putlib.sh LIBFILE: install muimaster.library on the instance at $WASABI_PORT and,
# if CLASSDIR is set (default: OpenMUI/ next to LIBFILE, if it exists), its class files
# (classfile.h) to LIBS:OpenMUI/. A build of before 2026-11-12 has them in Zune/ and
# its library looks in LIBS:Zune/: such a build's files go there.
LIB=$1
DRAWER=OpenMUI
if [ -z "${CLASSDIR:-}" ] && [ ! -d "$(dirname "$LIB")/OpenMUI" ] && [ -d "$(dirname "$LIB")/Zune" ]; then
  DRAWER=Zune
fi
CLASSDIR=${CLASSDIR:-$(dirname "$LIB")/$DRAWER}
wasabi put "$LIB" LIBS:muimaster.library --force >/dev/null
if [ -d "$CLASSDIR" ]; then
  wasabi run "makedir LIBS:$DRAWER" >/dev/null 2>&1
  for f in "$CLASSDIR"/*.mui; do wasabi put "$f" "LIBS:$DRAWER/$(basename "$f")" --force >/dev/null; done
fi
