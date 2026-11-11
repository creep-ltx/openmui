"""The library's symbol table for the checks, and the list of the names they look up.

The checks trap and call library functions by name. An LTO build inlines a
function into its callers unless it is told not to, and then a trap on it is
never reached. So every name a check looks up is recorded here and kept in
tests/trapped-symbols.txt; the build reads that list and marks those functions
noinline (gcc/trapped.py). A check that looks up a name the list lacks fails
with a message: run `make trapped-list` (on an LTO=0 build) to regenerate it.
"""
import atexit
import os
import sys
from pathlib import Path

LIST = Path(__file__).with_name('trapped-symbols.txt')
_seen = set()


class Symbols(dict):
    """name -> address, remembering every name looked up."""

    def __getitem__(self, name):
        _seen.add(name)
        return super().__getitem__(name)

    def get(self, name, default=None):
        _seen.add(name)
        return super().get(name, default)

    def __contains__(self, name):
        _seen.add(name)
        return super().__contains__(name)


def load(image, reloc, base):
    """The symbols of a loaded hunk image relocated to base."""
    return Symbols((s.name.decode(), address + s.offset)
                   for seg, address in zip(image.get_segments(), reloc.get_seq_addrs(base))
                   if seg.symtab for s in seg.symtab.symbols)


def _finish():
    log = os.environ.get('ZUNE68_SYMBOLS_LOG')
    if log:
        with open(log, 'a') as f:
            f.write(''.join(name + '\n' for name in sorted(_seen)))
        return
    if not LIST.exists():
        return
    missing = sorted(_seen - set(LIST.read_text().split()))
    if missing:
        print(f'{sys.argv[0]}: looks up {" ".join(missing)}, which {LIST.name} does not list; '
              'run `make trapped-list` on an LTO=0 build', file=sys.stderr)
        os._exit(1)


atexit.register(_finish)
