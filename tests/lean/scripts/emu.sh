# emu.sh: which emulator the lean scripts boot the test machines in (sourced by every script
# that starts one). EMU=amiberry (the default since 2026-11-11: Amiberry 8.5 headless, through
# amiberry-test next to this file) or EMU=fsuae (FS-UAE through amiga-test on PATH). Both
# runners take the same commands and options and work on the same copies in
# ~/.cache/amiga-test/NAME, so a script calls "amiga-test" as before and this function sends
# it to the chosen one. A script that runs another script passes EMU on in the environment.
EMU=${EMU:-amiberry}
case "$EMU" in
  amiberry) EMU_CMD="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/amiberry-test" ;;
  fsuae|fs-uae) EMU_CMD=amiga-test; EMU=fsuae ;;
  *) echo "EMU=$EMU: amiberry or fsuae" >&2; exit 2 ;;
esac
export EMU
amiga-test() { "$EMU_CMD" "$@"; }
