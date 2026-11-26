#!/usr/bin/env python3
"""The release version lives in VERSION only: no other file may hard-code a
different "OpenMUI X.Y[bN]", and the version lines of the docs must match."""
import re
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = (ROOT / 'VERSION').read_text().strip()
# History and records of what an older release measured or shipped
EXCLUDED = ('CHANGELOG.md', 'docs/lean/', 'docs/production.md', 'docs/stats/',
            'tests/', 'vendor/', 'LICENSE', 'license.html', 'ACKNOWLEDGEMENTS',
            'workbench/classes/zune/betterstring/', 'workbench/classes/zune/nlist/',
            'workbench/classes/zune/thebar')
MENTION = re.compile(rb'(?<!-)OpenMUI (\d+\.\d+(?:b\d+)?)(?![\w.]*\d)')
DOCLINE = re.compile(r'^\*\*Version (\S+?)\.\*\*', re.M)


class VersionTests(unittest.TestCase):
    def test_format(self):
        self.assertRegex(VERSION, r'^\d+\.\d+(b\d+)?$')

    def test_no_hardcoded_release_version(self):
        files = subprocess.run(['git', 'ls-files', '-z'], cwd=ROOT, check=True,
                               capture_output=True).stdout.decode().split('\0')
        bad = []
        for name in filter(None, files):
            if name.startswith(EXCLUDED) or not (ROOT / name).is_file():
                continue
            for found in MENTION.findall((ROOT / name).read_bytes()):
                if found.decode() != VERSION:
                    bad.append(f'{name}: OpenMUI {found.decode()}')
        self.assertEqual(bad, [], 'hard-coded release versions (edit VERSION '
                         'and use the placeholder or the macro instead)')

    def test_docs_state_the_version(self):
        for name in ('README.md', 'docs/user-guide.md'):
            found = DOCLINE.findall((ROOT / name).read_text())
            self.assertEqual(found, [VERSION], name)


if __name__ == '__main__':
    unittest.main()
