#!/usr/bin/env python3
"""Convert OpenMUI's Markdown documents to plain text for the Amiga.

The output is ISO-8859-1 text wrapped at 76 columns: headings underlined,
code blocks indented, tables as aligned columns (or, when too wide, one
block per row), links as text with the address in brackets, no Markdown
syntax left over.

  md2txt.py INPUT.md OUTPUT.txt
"""
import re
import sys
import textwrap
from pathlib import Path

WIDTH = 76

ASCII_FOR = {
    '→': '->', '←': '<-', '—': ' - ', '–': '-',
    '‘': "'", '’': "'", '“': '"', '”': '"',
    '…': '...', '×': 'x', '≤': '<=', '≥': '>=',
    '≈': '~', '≠': '!=', '•': '*', ' ': ' ',
}


def to_latin1(text):
    for char, repl in ASCII_FOR.items():
        text = text.replace(char, repl)
    return text.encode('latin-1', 'replace')


def inline(text):
    """Strip Markdown from one line of running text."""
    spans = []

    def keep(match):
        spans.append(match.group(1))
        return '\0%d\0' % (len(spans) - 1)

    text = re.sub(r'`([^`]*)`', keep, text)
    text = re.sub(r'!\[[^\]]*\]\([^)]*\)', '', text)

    def link(match):
        label, target = match.group(1), match.group(2)
        if re.match(r'https?://', target):
            return label if label == target else '%s (%s)' % (label, target)
        return label
    text = re.sub(r'\[([^\]]+)\]\(([^)]*)\)', link, text)
    text = re.sub(r'<(https?://[^>]+)>', r'\1', text)
    text = re.sub(r'\*\*(.+?)\*\*', r'\1', text)
    text = re.sub(r'(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])', r'\1', text)
    text = re.sub(r'(?<![\w_])_(?!\s)(.+?)(?<!\s)_(?![\w_])', r'\1', text)
    text = re.sub(r'<br ?/?>', ' ', text)
    text = re.sub(r'\\([\\`*_{}\[\]()#+\-.!|<>])', r'\1', text)
    text = re.sub(r'\0(\d+)\0', lambda m: spans[int(m.group(1))], text)
    return text


def wrap(text, indent='', first=None):
    first = indent if first is None else first
    lines = textwrap.wrap(text, WIDTH, initial_indent=first,
                          subsequent_indent=indent, break_long_words=False,
                          break_on_hyphens=False)
    return lines or ['']


def split_row(line):
    line = line.strip()
    if line.startswith('|'):
        line = line[1:]
    if line.endswith('|') and not line.endswith('\\|'):
        line = line[:-1]
    cells = re.split(r'(?<!\\)\|', line)
    return [inline(c.strip().replace('\\|', '|')) for c in cells]


def render_table(rows):
    header, body = rows[0], rows[1:]
    cols = len(header)
    body = [r + [''] * (cols - len(r)) for r in body]
    widths = [max(len(r[i]) for r in [header] + body) for i in range(cols)]
    if sum(widths) + 2 * (cols - 1) <= WIDTH and any(h for h in header):
        out = []
        for row in [header, None] + body:
            if row is None:
                out.append('  '.join('-' * w for w in widths))
            else:
                out.append('  '.join(c.ljust(w) for c, w in
                                     zip(row, widths)).rstrip())
        return out
    out = []
    for row in body:
        title = row[0] or '-'
        out += wrap(title)
        for head, cell in zip(header[1:], row[1:]):
            if not cell:
                continue
            label = (head + ': ') if head else ''
            out += wrap(label + cell, '      ', '    ')
        out.append('')
    return out[:-1] if out else out


def convert(source):
    lines = source.splitlines()
    out = []
    i = 0
    para = []

    def flush():
        if para:
            out.extend(wrap(inline(' '.join(para))))
            out.append('')
            para.clear()

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        if stripped.startswith('```'):
            flush()
            i += 1
            while i < len(lines) and not lines[i].strip().startswith('```'):
                out.append('    ' + lines[i].rstrip())
                i += 1
            out.append('')
            i += 1
            continue
        if re.match(r'\s*<!--.*-->\s*$', line):
            i += 1
            continue
        match = re.match(r'(#{1,6})\s+(.*)', line)
        if match:
            flush()
            level, title = len(match.group(1)), inline(match.group(2))
            if level == 1:
                out += [title, '=' * min(len(title), WIDTH), '']
            elif level == 2:
                out += [title, '-' * min(len(title), WIDTH), '']
            else:
                out += [title, '~' * min(len(title), WIDTH), '']
            i += 1
            continue
        if stripped.startswith('|') and i + 1 < len(lines) and re.match(
                r'\s*\|?\s*:?-{3,}', lines[i + 1]):
            flush()
            rows = [split_row(line)]
            i += 2
            while i < len(lines) and lines[i].strip().startswith('|'):
                rows.append(split_row(lines[i]))
                i += 1
            out += render_table(rows)
            out.append('')
            continue
        if stripped.startswith('>'):
            flush()
            quote = []
            while i < len(lines) and lines[i].strip().startswith('>'):
                quote.append(lines[i].strip().lstrip('>').strip())
                i += 1
            text = ' '.join(q for q in quote if q)
            out += wrap(inline(text), '  ')
            out.append('')
            continue
        match = re.match(r'(\s*)([-*]|\d+\.)\s+(.*)', line)
        if match:
            flush()
            depth = len(match.group(1)) // 2
            marker = match.group(2)
            marker = '-' if marker in '-*' else marker
            text = [match.group(3)]
            i += 1
            while (i < len(lines) and lines[i].strip()
                   and not re.match(r'\s*([-*]|\d+\.)\s+', lines[i])
                   and not lines[i].strip().startswith(('|', '```', '#', '>'))):
                text.append(lines[i].strip())
                i += 1
            pad = '  ' * depth
            hang = pad + ' ' * (len(marker) + 1)
            out += wrap(inline(' '.join(text)), hang, pad + marker + ' ')
            if not (i < len(lines) and re.match(r'\s*([-*]|\d+\.)\s+', lines[i])):
                out.append('')
            continue
        if not stripped:
            flush()
            i += 1
            continue
        para.append(stripped)
        i += 1
    flush()
    while out and out[-1] == '':
        out.pop()
    text = '\n'.join(out) + '\n'
    return re.sub(r'\n{3,}', '\n\n', text)


def convert_file(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(to_latin1(convert(source.read_text(encoding='utf-8'))))


if __name__ == '__main__':
    convert_file(Path(sys.argv[1]), Path(sys.argv[2]))
