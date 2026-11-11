#!/usr/bin/env python3
"""Assemble the native OpenMUI package (the lean edition of Zune68) from native GCC build outputs."""
import argparse
import re
import shutil
import struct
import subprocess
import tempfile
import tarfile
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from components import CLASS_FILES, EXAMPLES, LANGUAGES, catalog_paths

LIBRARIES = ('muimaster.library',)


def component_version(path):
    """The version a bundled component's $VER string reports for its name."""
    data = path.read_bytes()
    if data[:4] != struct.pack('>I', 1011):
        raise ValueError(f'{path}: expected an Amiga Hunk load file')
    pattern = rb'\$VER: ' + re.escape(path.name.encode()) + rb' (\d+)\.(\d+)\b'
    versions = set(re.findall(pattern, data))
    if len(versions) != 1:
        raise ValueError(f'{path}: missing or ambiguous matching $VER string')
    return '.'.join(part.decode() for part in versions.pop())


def library_version(path):
    """Return (library version, release date as YYYY.MM.DD, stage) from $VER.

    The library version is the one it reports to applications; releases
    are identified by the $VER date and the text after it: "Zune68 <stage>"
    for upstream's release lines (stage "Beta 2"), or an edition of its own
    such as "OpenMUI 0.1b1" (earlier: "Zune68k lean"), which is then the
    stage.
    """
    data = path.read_bytes()
    if data[:4] != struct.pack('>I', 1011):
        raise ValueError(f'{path}: expected an Amiga Hunk load file')
    pattern = (rb'\$VER: ' + re.escape(path.name.encode()) +
               rb' (\d+\.\d+) \((\d\d)\.(\d\d)\.(\d{4})\)(?: (Zune68[A-Za-z0-9 ]*?|OpenMUI [0-9][A-Za-z0-9.]*))?[\r\n\0]')
    versions = set(re.findall(pattern, data))
    if len(versions) != 1:
        raise ValueError(f'{path}: missing or ambiguous matching $VER string')
    version, day, month, year, stage = versions.pop()
    stage = stage.decode()
    if stage == 'Zune68' or stage.startswith('Zune68 '):
        stage = stage[len('Zune68 '):]
    return (version.decode(), f'{year.decode()}.{month.decode()}.{day.decode()}',
            stage)


def archive_name(date, stage):
    """Zune68-DATE-amigaos3-m68k.lha for upstream's release lines; an
    edition of its own goes by its name: OpenMUI-0.1b1-DATE-amigaos3-m68k.lha"""
    prefix = (stage.replace(' ', '-') if stage.startswith(('Zune68k', 'OpenMUI'))
              else 'Zune68')
    return f'{prefix}-{date}-amigaos3-m68k.lha'


def identity(build_dir):
    """The library's version, date and edition, used for release naming."""
    return library_version(build_dir / 'muimaster.library')


def substitute(text, version, date, stage):
    return (text.replace('[VERSION]', version).replace('[RELEASE]', date)
            .replace('[STAGE]', stage)
            .replace('[PLUGINS]', str(len(CLASS_FILES))))


def component_files(build_dir):
    files = ['Prefs/OpenMUI', 'Prefs/OpenMUI.info']
    files += ['Libs/MUI/' + name for name in CLASS_FILES]
    files += ['Examples/' + name for name in EXAMPLES]
    files += catalog_paths(build_dir)
    for directory in ['SDK', 'Docs']:
        entries = [p.relative_to(build_dir).as_posix()
                   for p in (build_dir / directory).rglob('*') if p.is_file()]
        if not entries:
            raise ValueError(f'{build_dir}: missing {directory}')
        files += entries
    for name in files:
        if not (build_dir / name).is_file():
            raise ValueError(f'{build_dir}: missing {name}')
    for name in ['Prefs/OpenMUI', *['Libs/MUI/' + n for n in CLASS_FILES]]:
        component_version(build_dir / name)
    return files


def builtin_class_files(build_dir):
    """The library's own classes built as files (LIBS:Zune, classfile.h)."""
    return sorted(p.name for p in (build_dir / 'Zune').glob('*.mui'))


def package_path(name):
    """Where a build file goes in the archive. Zune68's own .mui files
    (Pixmap.mui) go to Libs/Zune with the class files, not to Libs/MUI:
    LIBS:MUI is real MUI's drawer, and a Zune68 Pixmap.mui there would
    shadow (or replace) real MUI's own Pixmap.mui."""
    if name.startswith('Libs/MUI/') and name.endswith('.mui'):
        return 'Libs/Zune/' + name[len('Libs/MUI/'):]
    return name


def install_script(files, class_files=()):
    """dist/Install with the package check and the catalog lines filled in.

    The libraries, LIBS:Zune and the MUI classes are installed by the
    script itself (foreach over the package's drawers); here every file is
    checked to be in the package with a readable version, and each catalog
    gets its line, its language drawer named the Amiga way (german ->
    deutsch) in the first directory of LOCALE:."""
    script = (ROOT / 'dist/Install').read_text()
    files = [package_path(name) for name in files]
    files += ['Libs/Zune/' + name for name in class_files
              if 'Libs/Zune/' + name not in files]
    preflight = []
    for name in files:
        preflight.append('(if (<> (exists (tackon #package "' + name + '")) 1)\n'
                         '    (abort "The package is incomplete. Extract the complete archive again."))')
        if name == 'Prefs/OpenMUI' or name.startswith(('Libs/MUI/', 'Libs/Zune/')):
            preflight.append(f'(if (= (getversion (tackon #package "{name}")) 0)\n'
                             f'    (abort "Cannot read the bundled version of {name}."))')
    lines = []
    directories = set()
    for name in files:
        if name.startswith('Locale/Catalogs/'):
            parts = Path(name).parts[2:]
            language = LANGUAGES.get(parts[0], parts[0])
            relative = ['Catalogs', language, *parts[1:-1]]
            # Each drawer on the way, for old Installer and for the record
            for count in range(1, len(relative) + 1):
                parent = '/'.join(relative[:count])
                if parent not in directories:
                    lines.append(f'        (P_MakeDir (tackon #localedir "{parent}"))')
                    directories.add(parent)
            lines.append(f'        (P_Catalog "{name}" (tackon #localedir "{"/".join(relative)}"))')
    return (script.replace('; COMPONENT_PREFLIGHT', '\n'.join(preflight))
            .replace('; COMPONENT_CATALOGS', '\n'.join(lines)))


# Workbench icons for the SYS:OpenMUI drawer, made from the checked-in ones
WBDRAWER, WBTOOL, WBPROJECT = 2, 3, 4
NO_ICON_POSITION = -0x80000000


def make_icon(template, kind, position=None, default_tool=None, tool_types=(),
              stack=None):
    """A DiskObject (.info) with the template's images and drawer data and
    the given type, default tool, tool types and position."""
    data = template.read_bytes()
    if struct.unpack('>H', data[:2])[0] != 0xE310:
        raise ValueError(f'{template}: not a Workbench icon')
    (_, _, _, _, _, flags, _, _, render, select, _, _, _, _, userdata) = \
        struct.unpack('>IhhhhHHHIIIIIHI', data[4:48])
    drawer = struct.unpack('>I', data[66:70])[0]
    if drawer and kind != WBDRAWER:
        raise ValueError('a drawer template for an icon that is no drawer')
    offset = 78 + (56 if drawer else 0)
    for image in (render, select):
        if image:
            width, height, depth, pointer = struct.unpack('>hhhI', data[offset + 4:offset + 14])
            offset += 20 + (((width + 15) // 16) * 2 * height * depth if pointer else 0)
    header = bytearray(data[:offset])
    header[48] = kind
    struct.pack_into('>II', header, 50, 1 if default_tool else 0, 1 if tool_types else 0)
    if position is not None:
        struct.pack_into('>ii', header, 58, *position)
    struct.pack_into('>I', header, 70, 0)       # no tool window
    if stack is not None:
        struct.pack_into('>I', header, 74, stack)
    tail = b''

    def string(text):
        raw = text.encode('latin1') + b'\0'
        return struct.pack('>I', len(raw)) + raw
    if default_tool:
        tail += string(default_tool)
    if tool_types:
        tail += struct.pack('>I', (len(tool_types) + 1) * 4)
        for text in tool_types:
            tail += string(text)
    if drawer and userdata == 1:
        tail += b'\0' * 6                     # DrawerData2: flags, view modes
    return bytes(header) + tail


HOME_PROJECTS = {
    # file in SYS:OpenMUI: (ACTION, its text, icon position)
    'Use real MUI': ('MUI', 'OpenMUI: double-click the icon to use your real MUI\n'
                     'again (after a reboot). "Use OpenMUI" switches back.\n', (24, 8)),
    'Use OpenMUI': ('OPENMUI', 'OpenMUI: double-click the icon to use OpenMUI again\n'
                    '(after a reboot) after "Use real MUI".\n', (120, 8)),
    'Uninstall': ('UNINSTALL', 'OpenMUI: double-click the icon to remove OpenMUI and\n'
                  'put back every file of yours it had replaced.\n', (216, 8)),
}


def stage_home(package):
    """Home/: what the installer puts in SYS:OpenMUI besides the tool"""
    home = package / 'Home'
    home.mkdir()
    for name, (action, text, position) in HOME_PROJECTS.items():
        (home / name).write_text(text, encoding='latin1')
        (home / (name + '.info')).write_bytes(make_icon(
            ROOT / 'dist/OpenMUI-Prefs.info', WBPROJECT, position,
            default_tool='SYS:OpenMUI/Data/OpenMUISwitch',
            tool_types=(f'ACTION={action}',), stack=16384))
    readme = ROOT / 'dist/ReadMe.info'
    (home / 'ReadMe.info').write_bytes(make_icon(
        readme, WBPROJECT, (312, 8), default_tool='MultiView'))
    (home / 'OpenMUI.info').write_bytes(make_icon(
        ROOT / 'dist/OpenMUI.info', WBDRAWER, (NO_ICON_POSITION, NO_ICON_POSITION)))


def strip_binaries(package, strip):
    """Strip only staged Hunk executables, including libraries and plugins."""
    for path in sorted(package.rglob('*')):
        if not path.is_file():
            continue
        with path.open('rb') as source:
            is_hunk = source.read(4) == struct.pack('>I', 1011)
        if is_hunk:
            subprocess.run([strip, '--strip-all', str(path)], check=True)


def release(build_dir, output_dir, lha, full=True, strip='m68k-amigaos-strip'):
    build_dir, output_dir = build_dir.resolve(), output_dir.resolve()
    version, date, line = identity(build_dir)
    for name in ('opentest', 'Tools/OpenMUISwitch'):
        if not (build_dir / name).is_file():
            raise ValueError(f'{build_dir}: missing {name}')
    files = component_files(build_dir) if full else []
    if full and not (build_dir / "compatcheck").is_file():
        raise ValueError(f"{build_dir}: missing compatcheck")
    archiver = shutil.which(lha)
    if not archiver:
        raise ValueError(f'{lha}: LHA archiver not found; set LHA to its path')
    stripper = shutil.which(strip) if strip else None
    if strip and not stripper:
        raise ValueError(f'{strip}: Amiga strip tool not found; set STRIP or --strip')
    output_dir.mkdir(parents=True, exist_ok=True)
    archive = output_dir / archive_name(date, line)
    readme = substitute((ROOT / 'OpenMUI.readme').read_text(encoding='ascii'),
                        version, date, line)
    # Always assemble a fresh tree: removed files must not survive a release.
    # Publish only after archiving and CRC checking complete successfully.
    with tempfile.TemporaryDirectory(prefix='.openmui-', dir=output_dir) as temp:
        stage = Path(temp)
        package = stage / 'OpenMUI'
        (package / 'Libs').mkdir(parents=True)
        (package / 'Tests').mkdir()
        for name in LIBRARIES:
            shutil.copyfile(build_dir / name, package / 'Libs' / name)
            (package / 'Libs' / name).chmod(0o755)
        class_files = builtin_class_files(build_dir)
        if class_files:
            (package / 'Libs' / 'Zune').mkdir()
        for name in class_files:
            component_version(build_dir / 'Zune' / name)
            shutil.copyfile(build_dir / 'Zune' / name, package / 'Libs/Zune' / name)
            (package / 'Libs/Zune' / name).chmod(0o755)
        (package / 'Tools').mkdir()
        shutil.copyfile(build_dir / 'Tools/OpenMUISwitch', package / 'Tools/OpenMUISwitch')
        (package / 'Tools/OpenMUISwitch').chmod(0o755)
        stage_home(package)
        shutil.copyfile(build_dir / 'opentest', package / 'Tests/opentest')
        (package / 'Tests/opentest').chmod(0o755)
        if full:
            shutil.copyfile(build_dir / 'compatcheck', package / 'Tests/compatcheck')
            (package / 'Tests/compatcheck').chmod(0o755)
        shutil.copyfile(ROOT / 'LICENSE', package / 'LICENSE')
        (package / 'OpenMUI.readme').write_text(readme, encoding='ascii')
        for name in ('ReadMe.info', 'Install.info'):
            shutil.copyfile(ROOT / 'dist' / name, package / name)
        for name in ('ReadMe', 'Compatibility'):
            (package / name).write_text(substitute(
                (ROOT / 'dist' / name).read_text(encoding='ascii'),
                version, date, line), encoding='ascii')
        (package / 'Install').write_bytes(
            install_script(files, class_files).encode('latin1'))
        for name in files:
            target = package / package_path(name)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(build_dir / name, target)
            if name.startswith(('Libs/', 'Prefs/', 'Examples/')) and not name.endswith('.info'):
                target.chmod(0o755)
        drawers = ['Libs', 'Tests']
        if full:
            drawers += ['Prefs', 'Locale', 'Examples', 'SDK', 'Docs']
        for name in drawers:
            shutil.copyfile(ROOT / 'dist' / (name + '.info'),
                            package / (name + '.info'))
        if stripper:
            strip_binaries(package, stripper)
        if full:
            # Ship corresponding sources, including the native build adapters.
            source = stage / 'Source'
            ignore = shutil.ignore_patterns('*.o', '*.a', '*.map', '*.gst',
                                           '__pycache__', '.git', '*.rej', '.obj*', 'bin_*', '*.library',
                                           '*.lib', '*.lnk', 'opentest', 'buildincludes')
            for directory in ['workbench', 'compiler', 'developer', 'vendor']:
                shutil.copytree(ROOT / directory, source / directory, ignore=ignore)
            shutil.copytree(ROOT / 'tools', source / 'tools', ignore=ignore)
            shutil.copytree(ROOT / 'tests', source / 'tests', ignore=ignore)
            shutil.copytree(ROOT / 'dist', source / 'dist', ignore=ignore)
            shutil.copytree(ROOT / '.github', source / '.github')
            for name in ['GNUmakefile', 'LICENSE', 'LICENSE.Author', 'LICENSE.GPL',
                         'LICENSE.LGPL', 'LEGAL', 'ACKNOWLEDGEMENTS',
                         '.gitmodules', 'OpenMUI.readme']:
                shutil.copyfile(ROOT / name, source / name)
            if (ROOT / 'BUILD.md').exists():
                shutil.copyfile(ROOT / 'BUILD.md', source / 'BUILD.md')
            with tarfile.open(package / 'Source.tar.gz', 'w:gz') as archive_source:
                archive_source.add(source, arcname='Source')
        shutil.copyfile(ROOT / 'dist/OpenMUI.info', stage / 'OpenMUI.info')
        temporary_archive = stage / archive.name
        subprocess.run([archiver, 'ao5', str(temporary_archive),
                        'OpenMUI', 'OpenMUI.info'], cwd=stage, check=True)
        subprocess.run([archiver, 't', str(temporary_archive)], check=True)
        temporary_readme = stage / archive.with_suffix('.readme').name
        temporary_readme.write_text(readme, encoding='ascii')
        temporary_archive.replace(archive)
        temporary_readme.replace(archive.with_suffix('.readme'))
    print(f'Release: {archive}')
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path,
                        default=ROOT / 'build/muimaster')
    parser.add_argument('--output-dir', type=Path,
                        default=ROOT / 'build/muimaster/release')
    parser.add_argument('--lha', default='lha')
    parser.add_argument('--strip', default='m68k-amigaos-strip',
                        help='Amiga strip tool for staged executables')
    parser.add_argument('--no-strip', dest='strip', action='store_const', const=None,
                        help='retain symbols in the package for debugging')
    parser.add_argument('--core-only', action='store_true',
                        help='package only the two libraries and opentest')
    args = parser.parse_args()
    try:
        release(args.build_dir, args.output_dir, args.lha, full=not args.core_only,
                strip=args.strip)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'release: {error}\n')


if __name__ == '__main__':
    main()
