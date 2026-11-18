#!/usr/bin/env python3
"""Generate docs/reference/classes-*.md from the class sources.

Reads, for every built-in class in workbench/libs/muimaster/classes:
  - the header (attribute and method names, MUI version, ISG flags, types,
    method message structures),
  - the source (the _desc table: name and superclass; the dispatcher: the
    methods it handles; OM_NEW/OM_SET/OM_GET: which attributes are read, set
    and answered),
  - the autodoc blocks in the source (meaning, default, differences).
Nothing is written from memory.  Run from the repository root:

    python3 tools/gen_reference.py [--check]
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MM = ROOT / 'workbench/libs/muimaster'
CLS = MM / 'classes'
OUT = ROOT / 'docs/reference'

GROUPS = [
    ('classes-core.md', 'Core classes',
     'Notify, Family, Application, Window, Area, and the small data classes that belong to the object model.',
     ['Notify', 'Family', 'Application', 'Window', 'Area', 'Rectangle', 'Semaphore', 'Dataspace',
      'Configdata', 'Menuitem', 'Menu', 'Menustrip', 'Boopsi']),
    ('classes-layout.md', 'Layout classes',
     'Group and the classes derived from it, and the layout helpers.',
     ['Group', 'Virtgroup', 'Scrollgroup', 'Register', 'Balance', 'Selectgroup', 'Scrollbar', 'Scrollbutton']),
    ('classes-display.md', 'Display classes',
     'Classes that show something: text, images, frames, colours, gauges.',
     ['Text', 'Image', 'Bitmap', 'Bodychunk', 'ChunkyImage', 'Dtpic', 'Pixmap', 'Imagedisplay',
      'Framedisplay', 'Pendisplay', 'Gauge', 'Scale', 'Colorfield', 'Levelmeter', 'Crawling',
      ]),
    ('classes-input.md', 'Input classes',
     'Classes the user operates: strings, numbers, sliders, choices.',
     ['String', 'Prop', 'Numeric', 'Slider', 'Numericbutton', 'Knob', 'Cycle', 'Radio', 'Palette']),
    ('classes-lists.md', 'List classes',
     'List and the classes that show lists of text, volumes, directories.',
     ['List', 'Listview', 'Floattext', 'Volumelist', 'Dirlist', 'Scrmodelist']),
    ('classes-popups.md', 'Pop classes',
     'Popstring and the classes that open a window to choose a value.',
     ['Popstring', 'Popobject', 'Poplist', 'Popasl', 'Popscreen', 'Poppen', 'Popimage', 'Popframe']),
    ('classes-mui45.md', 'MUI 4 and MUI 5 classes',
     'Classes that only MUI 4 and MUI 5 have.  All are class files loaded on demand; a MUI 3 program pays nothing for them.',
     ['Title', 'Panel', 'Fontpanel', 'Fontdisplay', 'Filepanel', 'Selectorpanel', 'Keyadjust', 'Datamap',
      'Objectmap', 'Textdata', 'Process']),
    ('classes-prefs.md', 'Settings and adjust classes',
     'The classes the preferences editor (and `MUIM_Application_OpenConfigWindow`) is built from.  '
     'Programs rarely use them directly.',
     ['Settingsgroup', 'Settings', 'Aboutmui', 'Mccprefs', 'Imageadjust', 'Frameadjust', 'Penadjust',
      'Coloradjust']),
]

LEGEND = (
    '**How to read the tables.** ISG is the flag set from the header (the MUI 3.8 one for MUI 3 attributes): '
    'i = may be given to `NewObject`, s = may be set, g = may be read. A trailing `*` means the header has no '
    'flags (MUI 4/5 names) and the letters were found by reading OpenMUI\'s own OM_NEW (i), OM_SET (s) and '
    'OM_GET (g) code; a blank means the attribute is handled elsewhere (a superclass or a module) or its '
    'flags are not visible in the class file. "Since" is the interface version the header gives (empty for MUI 4/5 names, which carry none). "not handled" means no built source file '
    'mentions the attribute (it is accepted and ignored). Meaning and default come from the autodoc block in '
    'the class source where one exists; an empty meaning means there is none, and the attribute behaves as in '
    'MUI\'s own documentation. The messages are the structures after `MethodID`.\n')

# which header file holds a class's unprefixed names (MUIA_Background, MUIM_Draw ...)
OWN_HEADER = {'Notify': 'notify.h', 'Area': 'area.h', 'Window': 'window.h',
              'Application': 'application.h'}


def clean_comment(s):
    return re.sub(r'\s+', ' ', s).strip()


def read(path):
    return path.read_text(encoding='latin-1')


# ---------------------------------------------------------------- headers
ITEMS = {}      # name -> dict(kind, v, flags, type, file, comment)


def parse_headers():
    files = sorted(CLS.glob('*.h')) + [MM / 'mui.h']
    for f in files:
        text = read(f).replace('\\\n', ' ')
        for m in re.finditer(r'^[ \t]*#define[ \t]+(MUI([AM])_\w+)[ \t]+([^\n]*)$', text, re.M):
            name, kind, rest = m.group(1), m.group(2), m.group(3)
            if name in ITEMS and ITEMS[name]['v'] is not None:
                continue
            c = re.findall(r'/\*(.*?)\*/', rest)
            comment = ' '.join(c)
            v = flags = typ = None
            mm = re.search(r'(MUI|ZUNE):\s*V(\d+)\s*([isgIsG.]{3})?\s*([^*]*)', comment)
            if mm:
                v = ('V' + mm.group(2)) if mm.group(1) == 'MUI' else 'Zune'
                flags = mm.group(3)
                typ = clean_comment(mm.group(4)) or None
            if name in ITEMS and v is None:
                continue
            ITEMS[name] = dict(kind=kind, v=v, flags=flags, type=typ, file=f.name,
                               comment=clean_comment(comment))


STRUCTS = {}


def parse_structs():
    for f in sorted(CLS.glob('*.h')) + [MM / 'mui.h']:
        text = read(f)
        for m in re.finditer(r'struct\s+(MUIP_\w+)\s*\{(.*?)\}\s*;', text, re.S):
            body = re.sub(r'/\*.*?\*/', '', m.group(2), flags=re.S)
            fields = []
            for stmt in body.split(';'):
                stmt = clean_comment(stmt.replace('STACKED', ''))
                if not stmt or stmt.startswith('ULONG MethodID'):
                    continue
                fields.append(stmt)
            STRUCTS.setdefault(m.group(1), fields)


# ---------------------------------------------------------------- sources
def parse_autodocs(text):
    docs = {}
    for m in re.finditer(r'/\*{5,6} (\w+)\.mui/(\w+) \*+\n(.*?)\n\*/', text, re.S):
        body = [re.sub(r'^\*\s{0,3}', '', ln) for ln in m.group(3).split('\n')]
        secs = {}
        cur = None
        for ln in body:
            if re.match(r'^[A-Z][A-Z ]+$', ln.strip()) and not ln.startswith(' '):
                cur = ln.strip()
                secs[cur] = []
            elif cur:
                secs[cur].append(ln)
        docs[m.group(2)] = {k: clean_comment(' '.join(v)) for k, v in secs.items()}
    return docs


def first_sentence(s, limit=220):
    if not s:
        return ''
    m = re.match(r'(.+?[.!?])(\s|$)', s)
    out = m.group(1) if m else s
    if len(out) > limit:
        out = out[:limit].rsplit(' ', 1)[0] + ' ...'
    return out


def built_sources():
    mk = read(MM / 'GNUmakefile')
    return set(re.findall(r'classes/([a-z0-9]+)\.c', mk))


def parse_class_sources():
    classes = {}
    built = built_sources()
    for f in sorted(CLS.glob('*.c')):
        if f.stem not in built or f.stem.startswith('z'):
            continue
        text = read(f)
        docs = parse_autodocs(text)
        for m in re.finditer(r'_MUI_\w+_desc\s*=\s*\{\s*(MUIC_\w+)[^,]*,\s*(?:/\*.*?\*/\s*)*(MUIC_\w+|ROOTCLASS)[^,]*,'
                             r'[^,]*,\s*(?:/\*.*?\*/\s*)*(?:\(\w+ ?\*?\))?\s*&?(\w+)', text):
            cname = m.group(1)[5:]
            sup = m.group(2)
            dname = m.group(3)
            disp = re.search(r'BOOPSI_DISPATCHER\([^,]*,\s*' + dname + r'\b[^)]*\)\s*\{(.*?)BOOPSI_DISPATCHER_END',
                             text, re.S)
            methods = re.findall(r'case\s+((?:OM|MUIM)_\w+)\s*:', disp.group(1)) if disp else []
            pre = dname[:-len('_Dispatcher')] if dname.endswith('_Dispatcher') else cname
            funcs = {}
            for fm in re.finditer(r'^\w[\w \*]*?\b' + pre + r'__(OM_NEW|OM_SET|OM_GET)\s*\(', text, re.M):
                start = fm.end()
                nxt = re.search(r'^\}', text[start:], re.M)
                funcs.setdefault(fm.group(1), '')
                funcs[fm.group(1)] += text[start:start + (nxt.start() if nxt else 0)]
            classes[cname] = dict(file=f.name, super=sup, methods=methods,
                                  docs=docs, text=text, flags=scan_flags(text))
    return classes


def clsname(token):
    t = token.strip('"')
    if t.startswith('MUIC_'):
        return t[5:]
    return t.split('.')[0]


def classfiles():
    mk = read(MM / 'GNUmakefile')
    m = re.search(r'^CLASSFILES \?=(.*?)^CF_SOURCES', mk, re.S | re.M)
    return set(m.group(1).replace('\\', ' ').split())


ALLSRC = ''


def scan_flags(text):
    """attr -> letters I/S/G: the attribute is named inside OM_NEW, OM_SET or OM_GET code."""
    flags = {}
    mode = None
    for ln in text.split('\n'):
        if re.match(r'^[A-Za-z_]', ln) and '(' in ln and not ln.rstrip().endswith(';'):
            m = re.search(r'__OM_(NEW|SET|GET)\s*\(', ln)
            mode = {'NEW': 'I', 'SET': 'S', 'GET': 'G'}[m.group(1)] if m else None
        m = re.search(r'(?:case|==)\s+OM_(NEW|SET|GET)\b', ln)
        if m:
            mode = {'NEW': 'I', 'SET': 'S', 'GET': 'G'}[m.group(1)]
        elif re.search(r'case\s+(OM_DISPOSE|MUIM_\w+)\s*:', ln):
            mode = None
        if mode:
            for a in re.findall(r'\bMUIA_\w+', ln):
                flags.setdefault(a, set()).add(mode)
    return flags


def code_flags(cls, attr):
    return ''.join(c for c in 'ISG' if c in cls['flags'].get(attr, ()))


def items_for(cname, ctx):
    pa, pm = 'MUIA_%s_' % cname, 'MUIM_%s_' % cname
    own = OWN_HEADER.get(cname)
    attrs, meths = [], []
    for name, it in ITEMS.items():
        if name.startswith(pa) or (own and it['file'] == own and it['kind'] == 'A' and '_' not in name[5:]):
            if it['kind'] == 'A':
                attrs.append(name)
        if name.startswith(pm) or (own and it['file'] == own and it['kind'] == 'M' and '_' not in name[5:]):
            if it['kind'] == 'M':
                meths.append(name)
    return sorted(attrs), sorted(meths)


MEANINGS = {
 'MUIA_Title_Clickable': 'Tabs react to the mouse. Default TRUE.',
 'MUIA_Title_Closable': 'Tabs show a close gadget. Default FALSE.',
 'MUIA_Title_EventHandlerPriority': 'Priority of the tab event handler (an application can place its own before it). Default 0.',
 'MUIA_Title_Newable': 'The title shows a "new tab" gadget. Default FALSE.',
 'MUIA_Title_OnLastClose': 'What closing the last tab does: `MUIV_Title_OnLastClose_Remove` (0) or `_WindowAction` (1). Default 0.',
 'MUIA_Title_Position': 'Side the tabs are on: Top 0, Bottom 1, Left 2, Right 3. Default 0.',
 'MUIA_Title_Sortable': 'Tabs can be dragged to reorder. Default FALSE.',
 'MUIM_Title_Close': 'Close the tab given by the title button object.',
 'MUIM_Title_New': 'Create a new tab (what the "new" gadget calls).',
 'MUIM_Title_FindPage': 'Find the page number of a title button object.',
 'MUIM_Panel_Run': 'Run the requester modally in the given application and window; the result is TRUE when the user accepted.',
 'MUIA_Fontpanel_ShowCollection': 'Which fonts to list: `MUIV_Fontpanel_ShowCollection_All` (-1), FixedWidth 1, Bitmap 2, TrueType 4, User 8. Default -1.',
 'MUIA_Filepanel_AcceptPattern': 'Show only names matching the pattern (same as ASL `ASLFR_AcceptPattern`).',
 'MUIA_Filepanel_RejectPattern': 'Hide names matching the pattern (`ASLFR_RejectPattern`).',
 'MUIA_Filepanel_DoMultiSelect': 'Allow selecting several files; ignored in save mode (`ASLFR_DoMultiSelect`).',
 'MUIA_Filepanel_DoPatterns': 'Show a pattern gadget (`ASLFR_DoPatterns`).',
 'MUIA_Filepanel_DoSaveMode': 'Save mode (`ASLFR_DoSaveMode`).',
 'MUIA_Filepanel_Drawer': 'The drawer, set and read (`ASLFR_InitialDrawer`).',
 'MUIA_Filepanel_DrawersOnly': 'Choose drawers only (`ASLFR_DrawersOnly`).',
 'MUIA_Filepanel_File': 'The file name, set and read (`ASLFR_InitialFile`).',
 'MUIA_Filepanel_FilterDrawers': 'Apply the pattern to drawers too (`ASLFR_FilterDrawers`).',
 'MUIA_Filepanel_FilterFunc': 'User filter hook (`ASLFR_FilterFunc`).',
 'MUIA_Filepanel_Pattern': 'Initial pattern, default `#?` (`ASLFR_InitialPattern`).',
 'MUIA_Filepanel_RejectIcons': 'Hide `.info` files (`ASLFR_RejectIcons`).',
 'MUIA_Keyadjust_AllowDoubleClick': 'Accept a double click as a key. Default TRUE.',
 'MUIA_Keyadjust_AllowMouseEvents': 'Accept mouse events as keys. Default FALSE.',
 'MUIA_Keyadjust_AllowMultipleKeys': 'Accept several keys in one specification. Default TRUE.',
 'MUIA_Keyadjust_AllowTripleClick': 'Accept a triple click as a key. Default TRUE.',
 'MUIA_Keyadjust_Key': 'The key specification string (the contents of its string gadget).',
 'MUIA_Datamap_AutoLock': 'Lock the map around each method call. Default FALSE.',
 'MUIA_Datamap_CopyKeys': 'Copy the key strings given to the map. Default TRUE.',
 'MUIA_Datamap_Count': 'Number of entries.',
 'MUIA_Datamap_Pool': 'Memory pool to allocate from. Default none.',
 'MUIM_Datamap_Clear': 'Remove all entries.',
 'MUIM_Datamap_Find': 'Find the data stored under a key.',
 'MUIM_Datamap_Get': 'Get the data and its size stored under a key.',
 'MUIM_Datamap_Iterate': 'Step through the entries; the counter starts at 0.',
 'MUIM_Datamap_IterationKey': 'The key of the entry the counter points at.',
 'MUIM_Datamap_Remove': 'Remove the entry for a key.',
 'MUIM_Datamap_Set': 'Store data of a given length under a key.',
 'MUIA_Textdata_Contents': 'The text pointer. Default an empty string.',
 'MUIA_Textdata_Encoding': 'Encoding of the contents as an IANA MIBenum (`MIBENUM_ISO_8859_1` 4, `_UTF_8` 106, `_ISO_10646_UCS_2` 1000). Default 0.',
 'MUIA_Textdata_SourceBytes': 'Length of the text in bytes. Default -1 (zero terminated).',
 'MUIM_Textdata_Convert': 'Convert the contents to another encoding (MIBenum).',
 'MUIA_Process_AutoLaunch': 'Start the process when the object is created.',
 'MUIA_Process_Name': 'Name of the process. Default "Zune process".',
 'MUIA_Process_Priority': 'Priority of the process. Default: the creating task\'s.',
 'MUIA_Process_SourceClass': 'Class whose `MUIM_Process_Process` runs in the process (may be NULL).',
 'MUIA_Process_SourceObject': 'Object that receives `MUIM_Process_Process`. Default: the process object itself.',
 'MUIA_Process_StackSize': 'Stack size of the process.',
 'MUIA_Process_Task': 'The process\'s task pointer; NULL when it is not running.',
 'MUIM_Process_Kill': 'Ask the process to stop and wait at most `maxdelay` for it.',
 'MUIM_Process_Launch': 'Start the process.',
 'MUIM_Process_Process': 'Runs in the new process: the method to implement; `kill` is set when the process should stop.',
 'MUIM_Process_Signal': 'Send signals to the process.',
}


KEYWORDS = re.compile(r'MUI ?[345]|differs|not implemented|not supported|ignored|only for|not done', re.I)


def diff_note(doc):
    notes = ' '.join(doc.get(k, '') for k in ('NOTES', 'BUGS'))
    for s in re.split(r'(?<=[.!?])\s+', notes):
        if KEYWORDS.search(s):
            return first_sentence(s, 260)
    return ''


def esc(s):
    return s.replace('|', '\\|')


def render_class(cname, cls, files, stats):
    doc = cls['docs']
    out = []
    sup = clsname(cls['super']) if cls['super'] not in ('NULL', 'ROOTCLASS') else '(root)'
    where = ('class file `LIBS:OpenMUI/%s.mui`' % cname) if cname.lower() in files else 'built into `muimaster.library`'
    out.append('## %s\n' % cname)
    out.append('| | |\n|---|---|')
    out.append('| Name | `MUIC_%s` = `"%s.mui"` |' % (cname, cname))
    out.append('| Superclass | %s |' % ('`%s`' % sup if sup != '(root)' else sup))
    out.append('| Where | %s; source `classes/%s` |' % (where, cls['file']))
    out.append('')
    d = doc.get('MUIC_' + cname, {})
    desc = d.get('DESCRIPTION', '')
    if desc:
        out.append(first_sentence(desc, 700) + '\n')
        dn = diff_note(d)
        if dn:
            out.append('Difference: ' + dn + '\n')
    attrs, meths = items_for(cname, cls)
    if attrs:
        out.append('### Attributes\n')
        out.append('| Attribute | Since | ISG | Type | Meaning, default |')
        out.append('|---|---|---|---|---|')
        for a in attrs:
            it = ITEMS[a]
            cf = code_flags(cls, a)
            fl = it['flags'] or ''
            if fl:
                shown = fl
            elif cf:
                shown = cf.lower().replace('i', 'i').replace('s', 's').replace('g', 'g') + '*'
            else:
                shown = ''
            d = doc.get(a, {})
            meaning = first_sentence(d.get('FUNCTION', '') or d.get('DESCRIPTION', ''), 200)
            m = re.search(r'Default ([^.]*)\.', d.get('INPUTS', ''))
            if m:
                meaning += (' ' if meaning else '') + 'Default ' + m.group(1) + '.'
            meaning = meaning or MEANINGS.get(a, '')
            dn = diff_note(d)
            if dn and dn not in meaning:
                meaning += ' *' + dn + '*'
            status = ''
            if not cf and not re.search(r'\b' + a + r'\b', ALLSRC):
                status = 'not handled. '
                stats['unhandled'] += 1
            stats['attrs'] += 1
            out.append('| `%s` | %s | %s | %s | %s |' % (
                a, it['v'] or '', shown, esc(re.sub(r'\s+-\s.*$', '', it['type'] or '')), esc(status + meaning)))
        out.append('')
    if meths:
        out.append('### Methods\n')
        out.append('| Method | Since | Message (after MethodID) | Dispatched here | Meaning |')
        out.append('|---|---|---|---|---|')
        for mname in meths:
            it = ITEMS[mname]
            st = STRUCTS.get('MUIP_' + mname[5:], None)
            fields = '; '.join(st) if st else ('(none)' if st == [] else '')
            here = 'yes' if mname in cls['methods'] else 'inherited or in a module'
            d = doc.get(mname, {})
            meaning = first_sentence(d.get('FUNCTION', ''), 200) or MEANINGS.get(mname, '')
            dn = diff_note(d)
            if dn and dn not in meaning:
                meaning += ' *' + dn + '*'
            stats['meths'] += 1
            out.append('| `%s` | %s | %s | %s | %s |' % (mname, it['v'] or '', esc(fields), here, esc(meaning)))
        out.append('')
    if meths or cls['methods']:
        std = [m for m in cls['methods'] if m.startswith('OM_') or (m not in meths and m.startswith('MUIM_'))]
        if std:
            out.append('Also handled by the dispatcher: ' + ', '.join('`%s`' % m for m in std) + '.\n')
    return '\n'.join(out)


# ---------------------------------------------------------------- functions
def parse_sfd(path):
    funcs = []
    idx = 5
    varargs = False
    for ln in read(path).split('\n'):
        if ln.startswith('==varargs'):
            varargs = True
            continue
        if ln.startswith('==') or ln.startswith('*') or not ln.strip():
            continue
        m = re.match(r'(.*?)\b(\w+)\((.*)\)\s*\(([^)]*)\)\s*$', ln)
        if not m:
            continue
        ret, name, args, regs = m.group(1).strip(), m.group(2), m.group(3).strip(), m.group(4).strip()
        if varargs:
            funcs.append(dict(name=name, ret=ret, args=args, regs=regs, lvo=None, varargs=True))
            varargs = False
        else:
            funcs.append(dict(name=name, ret=ret, args=args, regs=regs, lvo=-idx * 6, varargs=False,
                              private=name.startswith('MUI_Priv') or name.startswith('MUIS_')))
            idx += 1
    return funcs


def parse_function_docs():
    docs = {}
    for f in sorted(MM.glob('*.c')):
        text = read(f)
        for m in re.finditer(r'/\*{30,}\n(.*?)\n\*{30,}/', text, re.S):
            blk = m.group(1)
            head = blk.split('FUNCTION')[0]
            names = re.findall(r'\bMUI[S]?_\w+', head)
            if not names:
                continue
            body = blk[len(head):]
            body = re.sub(r'^\s*/\*\s*', '', body)
            secs, cur = {}, None
            for ln in blk[len(head) - 0:].split('\n') if False else ('FUNCTION' + body.split('FUNCTION', 1)[1] if 'FUNCTION' in blk else '').split('\n'):
                t = ln.strip()
                if t in ('FUNCTION', 'INPUTS', 'RESULT', 'NOTES', 'BUGS', 'SEE ALSO', 'EXAMPLE'):
                    cur = t
                    secs[cur] = []
                elif cur:
                    secs[cur].append(ln.rstrip())
            docs.setdefault(names[0], secs)
    return docs


def render_text(lines):
    """Autodoc text lines to Markdown: argument lines become bullets, paragraphs are rejoined."""
    out, para = [], []
    base = None
    def flush():
        if para:
            out.append(' '.join(para))
            out.append('')
            para.clear()
    for ln in lines:
        if not ln.strip():
            flush()
            continue
        ind = len(ln) - len(ln.lstrip())
        if base is None:
            base = ind
        t = ln.strip()
        if re.match(r'^[\w., ]+ - ', t) and ind <= base + 1 or re.match(r'^MUIO_\w+:', t):
            flush()
            para.append('- ' + (('`%s`' % t.split(' - ')[0] + ' - ' + t.split(' - ', 1)[1]) if ' - ' in t and not t.startswith('MUIO_') else '**' + t.split(':')[0] + '**:' + t.split(':', 1)[1]))
        else:
            if para and para[0].startswith('- ') and ind > base + 1:
                para.append(t)
            elif para and not para[0].startswith('- ') or not para:
                para.append(t)
            else:
                para.append(t)
    flush()
    return '\n'.join(out).strip()


def render_functions():
    funcs = parse_sfd(MM / 'include/fd/muimaster_lib.sfd')
    docs = parse_function_docs()
    out = ['# muimaster.library functions\n',
           'Generated by `tools/gen_reference.py` from `include/fd/muimaster_lib.sfd` and the autodoc blocks in '
           '`workbench/libs/muimaster/mui_*.c` (edit those, then regenerate). Part of the [reference](README.md).\n',
           'Library base `_MUIMasterBase` (`struct Library *MUIMasterBase`), `libname muimaster.library`, '
           'bias 30. The offset is the library vector offset (LVO); the registers are those of the .sfd/.fd. '
           'A function with a variable-argument form (`...`) names it; that form is not a library '
           'vector: with `MUIMASTER_YES_INLINE_STDARG` defined the inline header makes it, otherwise the link '
           'library `-lmui` provides it.\n',
           '## Index\n', '| LVO | Function | Registers |', '|---|---|---|']
    for f in funcs:
        if f['varargs']:
            continue
        priv = ' (private, reserved)' if f['name'].startswith('MUI_Priv') else ''
        out.append('| %d | `%s`%s | %s |' % (f['lvo'], f['name'], priv, f['regs'] or '-'))
    out.append('')
    n = 0
    for f in funcs:
        if f['varargs'] or f['name'].startswith('MUI_Priv'):
            continue
        n += 1
        d = docs.get(f['name'], {})
        out.append('## %s\n' % f['name'])
        out.append('```c\n%s %s(%s)\n```\n' % (f['ret'], f['name'], f['args']))
        va = [g for g in funcs if g['varargs'] and g['name'] in (f['name'][:-1], f['name'] + 'Tags')]
        out.append('Offset %d, registers %s.%s\n' % (f['lvo'], f['regs'] or 'none',
                   ' Variable-argument form: `%s`.' % va[0]['name'] if va else ''))
        for sec in ('FUNCTION', 'INPUTS', 'RESULT', 'NOTES', 'BUGS'):
            if d.get(sec):
                out.append('**%s**\n' % sec.capitalize())
                out.append(render_text(d[sec]) + '\n')
        if not d:
            out.append('No autodoc in the source.\n')
    (OUT / 'functions.md').write_text('\n'.join(out) + '\n')
    return n


def main():
    global ALLSRC
    parse_headers()
    parse_structs()
    classes = parse_class_sources()
    files = classfiles()
    built = built_sources()
    ALLSRC = '\n'.join(read(p) for p in sorted(MM.glob('*.c')) + sorted(CLS.glob('*.c'))
                       if p.parent == MM or p.stem in built)
    OUT.mkdir(parents=True, exist_ok=True)
    stats = dict(classes=0, attrs=0, meths=0, unhandled=0)
    done = set()
    index = []
    for fname, title, intro, names in GROUPS:
        body = ['# %s\n' % title, intro + '\n',
                'Generated by `tools/gen_reference.py` from the class sources and headers. Do not edit by hand; '
                'edit the autodoc in the class source, then regenerate. Part of the [reference](README.md).\n',
                LEGEND]
        present = []
        for n in names:
            cls = classes.get(n)
            if not cls:
                body.append('## %s\n\nNot implemented: `MUIC_%s` is declared in the header but no class of that '
                            'name is built.\n' % (n, n))
                continue
            present.append(n)
            done.add(n)
            stats['classes'] += 1
            body.append(render_class(n, cls, files, stats))
        index.append((fname, title, present))
        body.insert(2, 'Classes: ' + ', '.join('[%s](#%s)' % (n, n.lower()) for n in present) + '\n')
        (OUT / fname).write_text('\n'.join(body) + '\n')
    nf = render_functions()
    print('functions documented:', nf)
    left = sorted(set(classes) - done)
    print(stats)
    print('classes not in a group:', left)
    for fname, title, present in index:
        print(fname, len(present), ' '.join(present))


if __name__ == '__main__':
    main()
