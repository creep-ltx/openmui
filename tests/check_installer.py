#!/usr/bin/env python3
"""Check the generated Installer script (dist/Install + tools/release.py)
on the host: balanced parentheses, only Installer 43 functions, every
procedure and variable defined, every package file it names present.

    check_installer.py BUILD_DIR

The script is run for real on the FS-UAE test machines (docs/lean/README.md,
"The installer"); this catches the mistakes that would only show there.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from release import (component_files, builtin_class_files, install_script,
                     package_path, HOME_PROJECTS)

# Installer 43.3's functions and statements used by the script (from the
# Installer 43 documentation; anything else is a typo or needs 44+)
BUILTINS = {
    'set', 'if', 'while', 'foreach', 'procedure', 'cat', 'tackon', 'exists',
    'getversion', 'getassign', 'getenv', 'expandpath', 'pathonly', 'fileonly',
    'substr', 'strlen', 'abort', 'exit', 'message', 'welcome', 'working',
    'complete', 'askbool', 'copyfiles', 'makedir', 'textfile', 'run',
    'onerror', 'and', 'or', 'not', 'bitand', '=', '<>', '<', '>', '<=', '>=',
    '+', '-', '*', '/',
    # parameters
    'prompt', 'help', 'choices', 'default', 'source', 'dest', 'newname',
    'nogauge', 'noreq', 'safe', 'append', 'all', 'quiet', 'optional',
    'askchoice', 'delete', 'rename', 'include', 'strlen',
}


def tokens(text):
    """(kind, value) for '(' ')' strings and atoms; comments dropped"""
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == ';':
            while i < n and text[i] != '\n':
                i += 1
        elif c in '()':
            yield c, c
            i += 1
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == '\\' else 1
            if j >= n:
                raise ValueError(f'unterminated string at {i}')
            yield 'str', text[i + 1:j]
            i = j + 1
        elif c.isspace():
            i += 1
        else:
            j = i
            while j < n and not text[j].isspace() and text[j] not in '()";':
                j += 1
            yield 'atom', text[i:j]
            i = j


def check(script, package=None):
    problems = []
    depth = 0
    heads = []
    sets, uses, procedures, calls = set(), set(), set(), set()
    previous = None
    args = set()
    toks = list(tokens(script))
    for k, (kind, value) in enumerate(toks):
        if kind == '(':
            depth += 1
        elif kind == ')':
            depth -= 1
            if depth < 0:
                problems.append('a ")" too many')
                depth = 0
        elif kind == 'atom':
            if previous == '(':
                heads.append(value)
                if value == 'set' and toks[k + 1][0] == 'atom':
                    sets.add(toks[k + 1][1])
                if value == 'procedure':
                    procedures.add(toks[k + 1][1])
                    j = k + 2
                    while toks[j][0] == 'atom' and toks[j][1].startswith('#'):
                        args.add(toks[j][1])
                        j += 1
            if value.startswith('#'):
                uses.add(value)
            if value.startswith('P_'):
                calls.add(value)
        previous = kind
    if depth:
        problems.append(f'{depth} "(" not closed')
    for head in heads:
        if head.startswith(('P_', '#', '@', '"')) or head.lstrip('-').isdigit():
            continue
        if head not in BUILTINS:
            problems.append(f'unknown function ({head} ...)')
    for name in sorted(calls - procedures):
        problems.append(f'procedure {name} is called but not defined')
    for name in sorted(uses - sets - args):
        problems.append(f'{name} is used but never set')
    if '; COMPONENT_' in script:
        problems.append('a COMPONENT_ line was not filled in')
    if package is not None:
        for name in set(re.findall(r'\(tackon #package "([^"]+)"\)', script)):
            if not (package / name).exists():
                problems.append(f'package file {name} is missing')
    return problems


def main():
    build = Path(sys.argv[1])
    files = component_files(build)
    script = install_script(files, builtin_class_files(build))
    problems = check(script)
    # the files the script installs from the package by name
    known = {package_path(n) for n in files} | {'Libs/zunemaster.library', 'Libs/muimaster.library',
                          'Tools/ZuneSwitch', 'ReadMe', 'ReadMe.info',
                          'Home/Zune68k.info', 'Home/ReadMe.info'}
    known |= {'Home/' + n for n in HOME_PROJECTS} | {'Home/' + n + '.info' for n in HOME_PROJECTS}
    known |= {'Libs/Zune', 'Libs/MUI'} | {'Libs/Zune/' + n for n in builtin_class_files(build)}
    for name in set(re.findall(r'\(tackon #package "([^"]+)"\)', script)):
        if name not in known:
            problems.append(f'the script names {name}, which the package does not have')
    for problem in problems:
        print('check_installer:', problem)
    if problems:
        sys.exit(1)
    print(f'check_installer: Install OK ({len(script.splitlines())} lines)')


if __name__ == '__main__':
    main()
