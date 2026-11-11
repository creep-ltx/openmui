#!/usr/bin/env bash
# snooplog.sh PORT OUT: keep a "wasabi snoop" on the instance at PORT, reconnecting after every reboot
# (suite.sh boots a fresh machine per program), and append its LoadSeg lines to OUT. Run it in the
# background next to a suite or fullcheck run, kill it afterwards; then
#   grep 'LIBS:Zune/.*= ok' OUT
# lists every class file and module (LIBS:Zune/*.mui) a program loaded, with the program's name.
# Not with YAM 2.10 on OS 3.1: every DOS call is traced, and YAM trying its 50 PNG theme pictures
# through datatypes then answers nothing for minutes (its error window stays, it does not quit).
export WASABI_HOST=127.0.0.1 WASABI_PORT=$1; OUT=$2
while true; do
  wasabi ping >/dev/null 2>&1 || { sleep 3; continue; }
  timeout 3600 wasabi snoop --ignore-wasabi --output full 2>/dev/null | grep --line-buffered 'LoadSeg' >> "$OUT"
  echo "--- $(date +%T) snoop ended" >> "$OUT"
  sleep 5
done
