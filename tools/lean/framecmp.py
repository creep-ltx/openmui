#!/usr/bin/env python3
# framecmp.py MAPS.md FRAMETHICK-OUTPUT thick|thin [zunetype=muitype ...]: compare framethick's frame maps
# with MUI 3.8's maps.md column (text pixels, pen 2, count as background); extra args compare
# Zune's type z with MUI's type m.
import re, sys
maps, out, col = sys.argv[1], sys.argv[2], sys.argv[3]
remap = dict(a.split('=') for a in sys.argv[4:])
ref = {}
cur = None
for line in open(maps):
    m = re.match(r'## type (\d+) (raised|recessed) \(spec (..)\)', line)
    if m:
        cur = m.group(3); ref[cur] = {'rows': [], 'add': None}; continue
    m = re.match(r'add \(frame\+spacing\): thick (.*), thin (.*)', line)
    if m and cur:
        ref[cur]['add'] = m.group(1) if col == 'thick' else m.group(2); continue
    if cur and re.match(r'^[WwBdK]{6}\.\.[WwBdK]{6} [WwBdK]{6}\.\.[WwBdK]{6}$', line.strip()):
        a, b = line.split()
        ref[cur]['rows'].append(a if col == 'thick' else b)
got = {}
cur = None
for line in open(out):
    m = re.match(r'## spec (..)2211 add (L\d+ T\d+ sw\d+ sh\d+) box', line)
    if m:
        cur = m.group(1); got[cur] = {'rows': [], 'add': m.group(2)}; continue
    if cur and re.match(r'^\S{6}\.\.\S{6}$', line.strip()):
        got[cur]['rows'].append(line.strip().replace('2', 'B'))
ok = 0
for spec in sorted(got):
    rspec = spec
    if spec[0] in remap:
        rspec = remap[spec[0]] + spec[1]
    if rspec not in ref:
        continue
    r, g = ref[rspec], got[spec]
    diffs = []
    if r['add'] != g['add']:
        diffs.append('add MUI %s Zune %s' % (r['add'], g['add']))
    if len(r['rows']) != len(g['rows']):
        diffs.append('rows MUI %d Zune %d' % (len(r['rows']), len(g['rows'])))
    else:
        for i, (a, b) in enumerate(zip(r['rows'], g['rows'])):
            if a != b:
                diffs.append('row %d MUI %s Zune %s' % (i, a, b))
    label = spec if rspec == spec else '%s(vs MUI %s)' % (spec, rspec)
    if diffs:
        print('%s: %d differences' % (label, len(diffs)))
        for d in diffs:
            print('   ', d)
    else:
        ok += 1
        print('%s: same' % label)
print('%d of %d the same' % (ok, len([s for s in got if (remap.get(s[0], s[0]) + s[1]) in ref])))
