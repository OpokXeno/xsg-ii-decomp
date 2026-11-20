#!/usr/bin/env python3
"""Apply the naming pass: stage copies of TUs, then global function and data names everywhere. Labels only (D1).

    python3 -B tools/tu/name_apply.py --dir .work/x2-names          # plan, writes nothing
    python3 -B tools/tu/name_apply.py --dir .work/x2-names --write  # under baseline:integration

Reads DIR/proposals/*.json and DIR/stage/<unit>/*.c. Only `high` and `medium` names apply; `low` stays in the
proposals. A name that collides with another proposal or an existing symbol is refused and listed, and that
function keeps its old name. Afterwards run: x2_scaffold.py, tu_build_manifest.py, tu_ledger.py, repin.py,
configure.py, then the gate, which alone proves the result.
"""
from __future__ import annotations

import argparse
import collections
import glob
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
UNITS = ('main', 'ov01', 'ov02')
IDENTIFIER = re.compile(r'[A-Za-z_][A-Za-z0-9_]*\Z')
PLACEHOLDER = re.compile(r'(?:func|runtime)_[0-9A-Fa-f]{8}\Z')
ADDRESS_LABEL = re.compile(r'[A-Za-z_][A-Za-z0-9_]*_[0-9A-Fa-f]{8}\Z')   # camera_001F9D58: a label carrying its VA
DATA_LABEL = re.compile(r'D_[0-9A-Fa-f]{6,8}\Z')
C_WORDS = set('auto break case char const continue default do double else enum extern float for goto if int long main '
              'register return short signed sizeof static struct switch typedef union unsigned void volatile while '
              'asm inline'.split())


def is_label(name):
    """Safe to change as a whole word anywhere. Only an all-lowercase word (`act`, `map`) can also be a local, a field or text."""
    return not re.fullmatch(r'[a-z]+', name)


STRING_LITERAL = re.compile(r'("(?:[^"\\\n]|\\.)*")')


def replace_names(text, mapping):
    """Labels change everywhere but inside string literals: a literal is original data (a native-table string, a profiler
    section name), and renaming its function never changes its bytes. A plain word (`act`, `map`) is also a local, a
    field and text, so only its function uses change."""
    if not mapping:
        return text
    labels = {old: new for old, new in mapping.items() if is_label(old)}
    words = {old: new for old, new in mapping.items() if not is_label(old)}

    def code(segment):
        segment = re.sub(r'\b[A-Za-z_][A-Za-z0-9_]*\b', lambda m: labels.get(m.group(), m.group()), segment)
        for old, new in words.items():
            segment = re.sub(rf'(?<![.>\w"]){re.escape(old)}(?=\s*\()', new, segment)        # definition, prototype, call
            segment = re.sub(rf'(INCLUDE_ASM\([^,\n]*,\s*){re.escape(old)}(?=\s*\))', rf'\g<1>{new}', segment)
        return segment

    parts = STRING_LITERAL.split(text)       # odd indexes are the literals
    return ''.join(part if index % 2 else code(part) for index, part in enumerate(parts))


def labels_only(mapping):
    return {old: new for old, new in mapping.items() if is_label(old)}


def replace_in_registry(value, mapping):
    """Registry documents: a label anywhere in a string or key; a plain word only as a whole string value."""
    if isinstance(value, dict):
        return {replace_in_registry(key, labels_only(mapping)): replace_in_registry(item, mapping) for key, item in value.items()}
    if isinstance(value, list):
        return [replace_in_registry(item, mapping) for item in value]
    if isinstance(value, str):
        if value in mapping:
            return mapping[value]
        labels = labels_only(mapping)
        return re.sub(r'\b[A-Za-z_][A-Za-z0-9_]*\b', lambda m: labels.get(m.group(), m.group()), value)
    return value


def symbol_table(unit):
    table = {}
    for line in (ROOT / f'config/symbols/{unit}.txt').read_text(errors='replace').splitlines():
        match = re.match(r'\s*([A-Za-z_][\w.]*)\s*=\s*(0x[0-9A-Fa-f]+)', line)
        if match:
            table[match.group(1)] = int(match.group(2), 16)
    return table


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dir', required=True)
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--report')
    parser.add_argument('--matched-only', action='store_true', help='refuse functions still in asm and data absent from the C sources')
    parser.add_argument('--replace-medium', action='store_true',
                        help='let a medium row replace an existing name when its kind is generic or wrong (review passes)')
    args = parser.parse_args()
    base = Path(args.dir)

    records, by_unit_name = {}, {}
    known = collections.defaultdict(set)
    for unit in UNITS:
        for path in sorted((ROOT / 'config/tu' / unit).glob('*.json')):
            record = json.loads(path.read_text())
            if record.get('id') and 'functions' in record:
                records[record['id']] = record
                for function in record['functions']:
                    by_unit_name[(unit, function['name'])] = function
                    known[function['name']].add(unit)
    symbols = {unit: symbol_table(unit) for unit in UNITS}
    for unit, table in symbols.items():
        for name in table:
            known[name].add(unit)

    c_words = set()
    if args.matched_only:
        for path in glob.glob(str(ROOT / 'src/*/*.[ch]')):
            c_words.update(re.findall(r'\bD_[0-9A-Fa-f]{6,8}\b', Path(path).read_text(errors='replace')))
    refused, low, wanted = [], 0, []
    for path in sorted(glob.glob(str(base / 'proposals/*.json'))):
        try:
            proposal = json.loads(Path(path).read_text(encoding='utf-8'))
        except json.JSONDecodeError as error:
            refused.append(dict(file=path, reason=f'bad JSON: {error}'))
            continue
        tu = proposal.get('tu')
        if tu not in records:
            refused.append(dict(file=path, reason=f'unknown TU {tu}'))
            continue
        unit = tu.split('/')[0]
        owned = {f['name'] for f in records[tu]['functions']}
        for kind in ('functions', 'data'):
            for row in proposal.get(kind) or []:
                old, new = row.get('old'), row.get('name')
                if row.get('confidence') not in ('high', 'medium'):
                    low += 1
                    continue
                reason = None
                if not (isinstance(new, str) and IDENTIFIER.match(new)) or new in C_WORDS:
                    reason = 'not an identifier'
                elif PLACEHOLDER.match(new) or re.fullmatch(r'D_[0-9A-Fa-f]{6,8}', new):
                    reason = 'still a placeholder'
                elif old == new:
                    continue
                elif kind == 'functions' and old not in owned:
                    reason = f'not a function of {tu}'
                elif args.matched_only and kind == 'functions' and by_unit_name[(unit, old)]['state'] != 'c':
                    reason = 'function is not matched'
                elif args.matched_only and kind == 'data' and DATA_LABEL.match(old) and old not in c_words:
                    reason = 'data not used by matched C'
                elif (kind == 'functions' and not ADDRESS_LABEL.match(old) and row.get('confidence') != 'high'
                      and not (args.replace_medium and row.get('kind') in ('generic', 'wrong'))):
                    reason = 'replacing an existing name needs high confidence'
                elif (kind == 'data' and not DATA_LABEL.match(old or '') and row.get('confidence') != 'high'
                      and not (args.replace_medium and row.get('kind') in ('generic', 'wrong'))):
                    reason = 'replacing an existing data name needs high confidence'
                elif kind == 'data' and not DATA_LABEL.match(old or '') and old not in symbols[unit] and old not in symbols['main']:
                    reason = 'unknown data symbol'
                if reason:
                    refused.append(dict(tu=tu, old=old, new=new, reason=reason))
                    continue
                if kind == 'data':
                    va = int(old[2:], 16) if DATA_LABEL.match(old) else symbols[unit].get(old, symbols['main'].get(old))
                    # A short label (D_61B380) lives only in the overlay's own undefined_syms list.
                    full_label = not DATA_LABEL.match(old) or re.fullmatch(r'D_[0-9A-Fa-f]{8}', old)
                    owner = unit if unit != 'main' and (va >= 0xA80000 or not full_label) else 'main'
                else:
                    owner = unit
                    va = int(by_unit_name[(unit, old)]['va'], 16)
                wanted.append(dict(kind=kind, tu=tu, unit=owner, va=va, old=old, new=new,
                                   confidence=row.get('confidence'), evidence=row.get('evidence', '')))

    # Same old label proposed twice (data shared by TUs): the same new name is one rename, a different one is a conflict.
    grouped = collections.defaultdict(list)
    for row in wanted:
        grouped[(row['unit'], row['old'])].append(row)
    chosen = []
    for key, rows in grouped.items():
        names = {row['new'] for row in rows}
        if len(names) == 1:
            chosen.append(rows[0])
        else:
            best = [r for r in rows if r['confidence'] == 'high']
            if len({r['new'] for r in best}) == 1:
                chosen.append(best[0])
            else:
                refused.append(dict(old=key[1], unit=key[0], reason=f'proposed as {sorted(names)}'))
    by_new = collections.defaultdict(list)
    for row in chosen:
        by_new[row['new']].append(row)
    # A name freed in the same plan (a swap or a chain: A -> B while B -> C) is available: one regex pass renames both.
    # Refusing one rename can take back the name another relied on, so settle the set to a fixed point.
    final = []
    for new, rows in by_new.items():
        if len(rows) > 1:
            refused.append(dict(new=new, reason='same name for different symbols',
                                olds=[f'{r["unit"]}:{r["old"]}' for r in rows]))
        else:
            final.append(rows[0])
    while True:
        freed = {row['old'] for row in final}
        blocked = [row for row in final if row['new'] in known and row['new'] not in freed]
        if not blocked:
            break
        for row in blocked:
            refused.append(dict(new=row['new'], old=row['old'], reason='name already exists'))
        final = [row for row in final if row not in blocked]

    mapping = {unit: {} for unit in UNITS}
    for row in final:
        if row['unit'] == 'main':
            for unit in UNITS:
                mapping[unit][row['old']] = row['new']
        else:
            mapping[row['unit']][row['old']] = row['new']

    stages = []
    for unit in UNITS:
        for path in sorted(glob.glob(str(base / 'stage' / unit / '*.c'))):
            published = ROOT / 'src' / unit / Path(path).name
            if published.is_file():
                stages.append((Path(path), published))
            else:
                refused.append(dict(stage=path, reason='no published source'))

    plan = dict(functions=sum(r['kind'] == 'functions' for r in final), data=sum(r['kind'] == 'data' for r in final),
                low_not_applied=low, stages=len(stages), refused=len(refused), write=bool(args.write))
    print(json.dumps(plan))
    report = dict(plan=plan, applied=final, refused=refused)
    if args.report:
        Path(args.report).write_text(json.dumps(report, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')
    for row in refused[:40]:
        print(json.dumps(row, ensure_ascii=False)[:200], file=sys.stderr)
    if not args.write:
        return 0

    for stage, published in stages:
        published.write_text(stage.read_text(encoding='utf-8', errors='surrogateescape'), encoding='utf-8',
                             errors='surrogateescape')
    trees = {unit: [*glob.glob(str(ROOT / f'src/{unit}/*.c')), *glob.glob(str(ROOT / f'src/{unit}/*.h'))] for unit in UNITS}
    trees['main'] += glob.glob(str(ROOT / 'include/*.h'))
    for unit in UNITS:      # include/<unit>/<tu>.h takes its own unit's names
        trees[unit] += glob.glob(str(ROOT / f'include/{unit}/*.h'))
    for unit, paths in trees.items():
        for path in paths:
            text = Path(path).read_text(encoding='utf-8', errors='surrogateescape')
            new_text = replace_names(text, mapping[unit])
            if new_text != text:
                Path(path).write_text(new_text, encoding='utf-8', errors='surrogateescape')

    for file_name, kind in (('function-names.json', 'functions'), ('data-names.json', 'data')):
        path = ROOT / 'config/symbols' / file_name
        document = json.loads(path.read_text(encoding='utf-8'))
        for row in final:
            if row['kind'] == kind:
                document['units'].setdefault(row['unit'], {})[f'0x{row["va"]:08x}'] = row['new']
        path.write_text(json.dumps(document, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')

    # Registries keyed by function name follow; history keeps its meaning under the new label.
    for file_name in ('config/tu/source-classes.json', 'config/objects/data-carves.json'):
        path = ROOT / file_name
        document = json.loads(path.read_text(encoding='utf-8'))
        for tu in list(document['tus']):
            unit = tu.split('/')[0]
            document['tus'][tu] = replace_in_registry(document['tus'][tu], mapping[unit])
        path.write_text(json.dumps(document, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')
    path = ROOT / 'config/tu/post-migration.json'
    document = json.loads(path.read_text(encoding='utf-8'))
    key = next(k for k, v in document.items() if isinstance(v, list))
    document[key] = [replace_in_registry(entry, mapping[entry['tu'].split('/')[0]]) for entry in document[key]]
    path.write_text(json.dumps(document, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    sys.exit(main())
