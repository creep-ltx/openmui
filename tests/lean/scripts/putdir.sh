#!/usr/bin/env bash
# putdir.sh LOCALDIR REMOTEDIR: copy a folder tree to the instance at $WASABI_PORT
set -u
L=$1; R=$2
wasabi mkdir "$R" >/dev/null 2>&1
( cd "$L" && find . -type d | sed 's|^\./||' | grep -v '^\.$' ) | while read -r d; do wasabi mkdir "$R/$d" >/dev/null 2>&1; done
( cd "$L" && find . -type f | sed 's|^\./||' ) | while read -r f; do wasabi put "$L/$f" "$R/$f" --force >/dev/null 2>&1 || echo "failed: $f"; done
