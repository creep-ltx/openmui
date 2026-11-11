#!/usr/bin/env python3
"""Exercise release input failures and archive replacement on the host."""
import importlib.util
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release', ROOT / 'tools/release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
from prepare_aminet import prepare


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='zune68-release-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / 'objects with spaces'
        self.output = self.root / 'release with spaces'
        self.build.mkdir()
        self.output.mkdir()
        for name in release.LIBRARIES:
            self.library(name, '35.6')
        self.library('opentest', '1.0')
        (self.build / 'Tools').mkdir()
        self.library('Tools/OpenMUISwitch', '1.0')

    def library(self, name, version, date='12.10.2026', stage=' Zune68 Beta 1'):
        code = b'\x70\x00\x4e\x75' + f'$VER: {name} {version} ({date}){stage}\r\n\0'.encode()
        code += b'\0' * (-len(code) % 4)
        symbol = b'fixture_symbol\0\0'
        words = len(code) // 4
        header = struct.pack('>8I', 1011, 0, 1, 0, 0, words, 1001, words)
        symbols = struct.pack('>2I', 1008, len(symbol) // 4) + symbol
        (self.build / name).write_bytes(header + code + symbols +
                                       struct.pack('>3I', 0, 0, 1010))

    def test_missing_date(self):
        self.library('muimaster.library', '35.6', date='test')
        with self.assertRaisesRegex(ValueError, 'matching'):
            release.release(self.build, self.output, 'lha', full=False)

    def test_release_identity_from_version_string(self):
        self.assertEqual(release.identity(self.build), ('35.6', '2026.10.12', 'Beta 1'))
        for name in release.LIBRARIES:
            self.library(name, '35.6', stage='')
        self.assertEqual(release.identity(self.build), ('35.6', '2026.10.12', ''))
        archive = release.release(self.build, self.output, 'lha', full=False)
        self.assertEqual(archive.name, 'Zune68-2026.10.12-amigaos3-m68k.lha')
        readme = archive.with_suffix('.readme').read_text()
        self.assertIn('Version:      2026.10.12 - \n', readme)
        self.assertIn('muimaster.library 35.6', readme)
        self.assertNotIn('[', readme.split('\n\n', 1)[0])

    def test_lean_edition_names_the_archive(self):
        for name in release.LIBRARIES:
            self.library(name, '21.227', date='11.11.2026', stage=' OpenMUI 0.1b1')
        self.assertEqual(release.identity(self.build),
                         ('21.227', '2026.11.11', 'OpenMUI 0.1b1'))
        archive = release.release(self.build, self.output, 'lha', full=False)
        self.assertEqual(archive.name, 'OpenMUI-0.1b1-2026.11.11-amigaos3-m68k.lha')
        readme = archive.with_suffix('.readme').read_text()
        self.assertIn('2026.11.11', readme)
        destination = self.root / 'aminet'
        prepare(self.output, destination, 'v2026.11.11')
        self.assertEqual((destination / 'OpenMUI-0.1b1.lha').read_bytes(),
                         archive.read_bytes())

    def test_switch_tool_and_home_icons(self):
        archive = release.release(self.build, self.output, 'lha', full=False)
        listing = subprocess.check_output(['lha', 'l', str(archive)]).decode('latin1')
        for member in ['OpenMUI/Tools/OpenMUISwitch', 'OpenMUI/Home/Uninstall.info',
                       'OpenMUI/Home/Use real MUI.info', 'OpenMUI/Home/OpenMUI.info']:
            self.assertIn(member, listing)
        icon = subprocess.check_output(['lha', 'pq', str(archive),
                                        'OpenMUI/Home/Use real MUI.info'])
        self.assertEqual(icon[:2], b'\xe3\x10')
        self.assertEqual(icon[48], release.WBPROJECT)
        self.assertIn(b'SYS:OpenMUI/Data/OpenMUISwitch\0', icon)
        self.assertIn(b'ACTION=MUI\0', icon)

    def test_package_readmes_name_the_release(self):
        archive = release.release(self.build, self.output, 'lha', full=False)
        for member in ['OpenMUI/OpenMUI.readme', 'OpenMUI/ReadMe', 'OpenMUI/Compatibility']:
            text = subprocess.check_output(['lha', 'pq', str(archive), member]).decode('ascii')
            self.assertNotIn('[VERSION]', text)
            self.assertNotIn('[RELEASE]', text)
            self.assertNotIn('[STAGE]', text)
            self.assertIn('2026.10.12', text)
            self.assertIn('35.6', text)
        readme = subprocess.check_output(['lha', 'pq', str(archive), 'OpenMUI/OpenMUI.readme']).decode('ascii')
        self.assertIn('Version:      2026.10.12 - Beta 1\n', readme)

    def test_missing_version(self):
        (self.build / 'muimaster.library').write_bytes(struct.pack('>I', 1011))
        with self.assertRaisesRegex(ValueError, 'matching'):
            release.release(self.build, self.output, 'lha', full=False)

    def test_wrong_library_name(self):
        self.library('muimaster.library', '35.6')
        (self.build / 'muimaster.library').write_bytes(
            (self.build / 'muimaster.library').read_bytes().replace(
                b'$VER: muimaster.library', b'$VER: zunemaster.library'))
        with self.assertRaisesRegex(ValueError, 'matching'):
            release.release(self.build, self.output, 'lha', full=False)

    def test_one_library(self):
        """OpenMUI ships muimaster.library only (zunemaster.library is gone),
        even when an old build directory still has a zunemaster.library."""
        shutil.copyfile(self.build / 'muimaster.library',
                        self.build / 'zunemaster.library')
        archive = release.release(self.build, self.output, 'lha', full=False)
        listing = subprocess.check_output(['lha', 'l', str(archive)]).decode('latin1')
        self.assertIn('OpenMUI/Libs/muimaster.library', listing)
        self.assertNotIn('zunemaster', listing)

    def test_missing_tool(self):
        with self.assertRaisesRegex(ValueError, 'archiver not found'):
            release.release(self.build, self.output, str(self.root / 'absent'), full=False)

    def test_missing_strip_tool(self):
        with self.assertRaisesRegex(ValueError, 'strip tool not found'):
            release.release(self.build, self.output, 'lha', full=False,
                            strip=str(self.root / 'absent'))
        self.assertEqual(list(self.output.iterdir()), [])

    def test_package_strips_copies_and_keeps_versions(self):
        originals = {p.name: p.read_bytes() for p in self.build.iterdir() if p.is_file()}
        archive = release.release(self.build, self.output, 'lha', full=False)
        for name, original in originals.items():
            member = 'OpenMUI/' + ('Tests/' if name == 'opentest' else 'Libs/') + name
            data = subprocess.check_output(['lha', 'pq', str(archive), member])
            self.assertNotIn(b'fixture_symbol', data)
            self.assertIn(b'$VER: ' + name.encode(), data)
            self.assertEqual((self.build / name).read_bytes(), original)
        icon = subprocess.check_output(['lha', 'pq', str(archive), 'OpenMUI/Libs.info'])
        self.assertEqual(icon, (ROOT / 'dist/Libs.info').read_bytes())

    def test_strip_failure_preserves_previous_release(self):
        archive = self.output / 'Zune68-2026.10.12-amigaos3-m68k.lha'
        archive.write_bytes(b'previous release')
        with self.assertRaises(subprocess.CalledProcessError):
            release.release(self.build, self.output, 'lha', full=False,
                            strip='/usr/bin/false')
        self.assertEqual(archive.read_bytes(), b'previous release')
        self.assertEqual(list(self.output.iterdir()), [archive])

    def test_debug_package_keeps_symbols(self):
        archive = release.release(self.build, self.output, 'lha', full=False,
                                  strip=None)
        data = subprocess.check_output(['lha', 'pq', str(archive),
                                        'OpenMUI/Tests/opentest'])
        self.assertEqual(data, (self.build / 'opentest').read_bytes())

    def test_incomplete_full_package(self):
        for name in ['SDK/include/example.h', 'Docs/COPYING']:
            path = self.build / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'fixture')
        for name in ['Prefs/OpenMUI', 'Prefs/OpenMUI.info']:
            with self.assertRaisesRegex(ValueError, 'missing ' + name):
                release.release(self.build, self.output, 'lha')
            path = self.build / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'fixture')
        with self.assertRaisesRegex(ValueError, 'missing Libs/MUI/BetterString.mcc'):
            release.release(self.build, self.output, 'lha')
        self.assertEqual(list(self.output.iterdir()), [])

    def test_archiver_failure_preserves_previous_release(self):
        archive = self.output / 'Zune68-2026.10.12-amigaos3-m68k.lha'
        archive.write_bytes(b'previous release')
        with self.assertRaises(release.subprocess.CalledProcessError):
            release.release(self.build, self.output, '/usr/bin/false', full=False)
        self.assertEqual(archive.read_bytes(), b'previous release')
        self.assertEqual(list(self.output.iterdir()), [archive])

    def test_aminet_pair_from_generated_alpha_release(self):
        archive = release.release(self.build, self.output, 'lha', full=False)
        destination = self.root / 'aminet'
        prepare(self.output, destination, 'v2026.10.12')
        self.assertEqual((destination / 'Zune68.lha').read_bytes(), archive.read_bytes())
        self.assertEqual((destination / 'Zune68.readme').read_bytes(),
                         archive.with_suffix('.readme').read_bytes())

    def test_aminet_pair_and_version_guard(self):
        archive = self.output / 'Zune68-2026.10.12-amigaos3-m68k.lha'
        archive.write_bytes(b'archive fixture')
        readme = archive.with_suffix('.readme')
        readme.write_text('Short: Zune68\nType: util/libs\nVersion: 2026.10.12\n\nBody\n')
        destination = self.root / 'aminet'
        prepare(self.output, destination, 'v2026.10.12')
        self.assertEqual((destination / 'Zune68.lha').read_bytes(), archive.read_bytes())
        self.assertEqual((destination / 'Zune68.readme').read_bytes(), readme.read_bytes())
        original = (destination / 'Zune68.readme').read_bytes()
        for version in ['2026.10.11', '2026.10.11 - Beta 1', '19.81 - Beta 1',
                        '2026.10.12garbage', '2026.10.12 - ']:
            with self.subTest(version=version):
                readme.write_text(f'Type: util/libs\nVersion: {version}\n')
                with self.assertRaisesRegex(ValueError, 'version differ'):
                    prepare(self.output, destination, 'v2026.10.12')
                self.assertEqual((destination / 'Zune68.readme').read_bytes(), original)
        for tag in ['../v2026.10.12', 'v19.81', 'v2026.10.12.1']:
            with self.assertRaisesRegex(ValueError, 'date tag'):
                prepare(self.output, destination, tag)


if __name__ == '__main__':
    unittest.main()
