#!/usr/bin/env python3
"""trapped.py OUT.h LIST [--noinline REGEX] SOURCE... : the header that keeps the functions the
checks trap out of line in an LTO build.

LIST (tests/trapped-symbols.txt) names every symbol the vamos checks
(tests/check_*.py) look up; tests/symbols.py records and enforces it. For
each name defined as a function in one of the SOURCEs (the objects that go
through the LTO link) this writes a declaration carrying noinline, noclone,
no_icf, used and externally_visible, so that every call goes through the
function's own entry point, where the check's trap sits, and the symbol stays
in the library. Names defined elsewhere (the gates, the library init, libnix,
the class files, the prefs program) are not here.

The declaration has no prototype, `TYPE name();`, which needs only the return
type and is compatible with any definition whose parameters are int or wider
(C17 6.7.6.3p15). A definition with a narrower parameter (BOOL, WORD, UBYTE,
...) or `...` gets its parameter list copied instead, with a forward
declaration of each struct tag in it; the header is included after
clib/muimaster_protos.h, so the OS and MUI types are known by then. The
compiler reports any declaration that does not match its definition.

Definitions are found as `TYPE name(` at the start of a line (TYPE possibly
on the line before) or as the first two arguments of the AROS_LH*, AROS_UFH*
and BOOPSI_DISPATCHER macros.
"""
import re
import sys
from pathlib import Path

MACRO = re.compile(r'^\s*(?:AROS_LH\d+|AROS_UFH\d+|BOOPSI_DISPATCHER)\s*\(\s*([^,]+?)\s*,\s*(\w+)\s*,', re.M)
# TYPE name( at the start of a line (TYPE on that line or the one before).
PLAIN = re.compile(r'^([A-Za-z_][\w \t\*]*?)[ \t\n]*\b(\w+)[ \t]*\(', re.M)
NARROW = re.compile(r'\b(BOOL|WORD|UWORD|BYTE|UBYTE|short|char|float|TEXT)\b')
KEYWORDS = {'if', 'while', 'for', 'switch', 'return', 'sizeof', 'else', 'case', 'do'}
# TYPE name = or TYPE name[...] = at the start of a line: a data definition.
DATA = re.compile(r'^((?:const[ \t]+)?(?:struct[ \t]+\w+|\w+)(?:[ \t]+|[ \t]*\*+[ \t]*))(\w+)[ \t]*(\[[^\]]*\])?[ \t]*[=;]', re.M)
# A module file renames what it includes: #define name newname
RENAME = re.compile(r'^#define[ \t]+(\w+)[ \t]+(\w+)[ \t]*$', re.M)
INCLUDE = re.compile(r'^#include[ \t]+"([^"]+\.c)"', re.M)


def parameters(text, start):
    """The text between the '(' at start and its matching ')'."""
    depth, i = 0, start
    while i < len(text):
        if text[i] == '(':
            depth += 1
        elif text[i] == ')':
            depth -= 1
            if depth == 0:
                return text[start + 1:i]
        i += 1
    return None


def definitions(source):
    # BOOPSI_DISPATCHER_END is '}', and a definition may follow it on the line
    text = open(source, errors='replace').read().replace('BOOPSI_DISPATCHER_END ', '}\n').replace('BOOPSI_DISPATCHER_END', '}')
    found = {}
    # The sources a module file includes, under the names it gives them
    renames = dict(RENAME.findall(text))
    for included in INCLUDE.findall(text):
        for name, value in definitions(Path(source).parent / included).items():
            found.setdefault(renames.get(name, name), value)
    for m in MACRO.finditer(text):
        found[m.group(2)] = (m.group(1), None)
    for m in PLAIN.finditer(text):
        ret, name = m.group(1).strip(), m.group(2)
        words = ret.replace('*', ' ').split()
        if not words or words[0] in ('static', 'typedef', 'extern', 'return', 'else') or name in KEYWORDS:
            continue
        if ret.startswith('#') or any(w in KEYWORDS for w in words):
            continue
        params = parameters(text, m.end() - 1)
        if params is None:
            continue
        # A definition's body follows the ')'; a declaration has ';'.
        after = text[m.end() + len(params):m.end() + len(params) + 40].lstrip(')').lstrip()
        if not after.startswith('{'):
            continue
        found.setdefault(name, (ret, ' '.join(params.split())))
    for m in DATA.finditer(text):
        ret, name = m.group(1).strip(), m.group(2)
        if ret.split()[0] in ('static', 'return', 'extern', 'typedef', 'goto') or name in found:
            continue
        found[name] = (ret, ('[]' if m.group(3) else '') + '=')
    return found


def typedefs(sources):
    """Struct typedefs of the sources' headers: the name -> 'struct tag'."""
    names = {}
    for folder in {Path(s).parent for s in sources}:
        for header in folder.glob('*.h'):
            for tag, name in re.findall(r'^typedef\s+struct\s+(\w+)\s+(\w+)\s*;', header.read_text(errors='replace'), re.M):
                names[name] = f'struct {tag}'
    return names


def declaration(name, ret, params, names):
    # The library's own struct typedefs are not declared yet: use the tags.
    pattern = re.compile(r'\b(' + '|'.join(map(re.escape, names)) + r')\b') if names else None
    data = params is not None and params.endswith('=')
    if params is None or (not data and not NARROW.search(params) and '...' not in params):
        params = ''
    if pattern:
        ret, params = (pattern.sub(lambda m: names[m.group(1)], t) for t in (ret, params))
    tags = set(re.findall(r'\bstruct\s+(\w+)', params + ' ' + ret))
    if data:
        return [f'struct {tag};' for tag in sorted(tags)] + [f'extern {ret} {name}{params[:-1]} ZUNE_VISIBLE;']
    return [f'struct {tag};' for tag in sorted(tags)] + [f'{ret} {name}({params}) ZUNE_TRAPPED;']


def main():
    out, listfile, sources = sys.argv[1], sys.argv[2], sys.argv[3:]
    noinline = None
    if sources and sources[0] == '--noinline':
        noinline, sources = re.compile(sources[1]), sources[2:]
    wanted = {n[1:] for n in open(listfile).read().split() if n.startswith('_')}
    defined, kept = {}, {}
    for source in sources:
        for name, (ret, params) in definitions(source).items():
            if name in wanted:
                defined.setdefault(name, (ret, params, source))
            elif noinline and noinline.fullmatch(name) and params is not None and not params.endswith('='):
                kept.setdefault(name, (ret, params, source))
    names = typedefs(sources)
    lines = ['/* Generated by gcc/trapped.py from tests/trapped-symbols.txt: the',
             '   functions the checks trap stay out of line (see that file). */',
             '#define ZUNE_TRAPPED __attribute__((noinline, noclone, no_icf, used, externally_visible))',
             '#define ZUNE_VISIBLE __attribute__((externally_visible))']
    for name in sorted(defined):
        ret, params, source = defined[name]
        lines.append(f'/* {source} */')
        lines += declaration(name, ret, params, names)
    if kept:
        lines += ['/* Methods (--noinline): each stays a function of its own, as without LTO,',
                  '   so that a dispatcher never carries the frame of the one method it',
                  '   alone calls. */',
                  '#define ZUNE_NOINLINE __attribute__((noinline))']
        for name in sorted(kept):
            ret, params, source = kept[name]
            lines += [d.replace('ZUNE_TRAPPED', 'ZUNE_NOINLINE') for d in declaration(name, ret, params, names)]
    with open(out, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'{out}: {len(defined)} of {len(wanted)} listed names are defined in these sources, '
          f'{len(kept)} more noinline')


if __name__ == '__main__':
    main()
