#!/usr/bin/env bash
# putlib.sh LIBFILE: install muimaster.library on the instance at $WASABI_PORT and,
# if CLASSDIR is set (default: mui/ next to LIBFILE, if it exists), its class files to LIBS:mui/
LIB=$1
CLASSDIR=${CLASSDIR:-$(dirname "$LIB")/mui}
wasabi put "$LIB" LIBS:muimaster.library --force >/dev/null
if [ -d "$CLASSDIR" ]; then
  wasabi run "makedir LIBS:mui" >/dev/null 2>&1
  for f in "$CLASSDIR"/*.mui; do wasabi put "$f" "LIBS:mui/$(basename "$f")" --force >/dev/null; done
fi
