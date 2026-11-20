#!/usr/bin/env python3
"""Function table and translation-unit map of a stripped EE image, by inference.

Xenosaga II keeps no symbols, so neither function starts nor object boundaries can be
read from the ELF. This tool infers both from the bytes and measures its own error
on Xenosaga I, where the symbols give the truth (reports/x2-tu-map.md):

    map_infer.py functions               score the function estimator against X1 symbols
    map_infer.py calibrate               score boundary inference on X1, write the model
    map_infer.py generate                write the X2 TU maps from that model
    map_infer.py check                   verify the maps against the originals and each other
    map_infer.py report                  write reports/x2-tu-map.md from the model and the maps

The X1 tree is only read. Nothing here touches originals, source or the queue. The output
is analysis: every boundary and name is a hypothesis with its evidence, never proof.
Only calibrate needs numpy and the X1 tree; generate uses the model file.
"""
import argparse
import bisect
import collections
import hashlib
import json
import math
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
X1_ROOT = Path('/home/pc/xenosaga1-port/xenosaga-i-decomp')
PAD = 0
RA = 31
GP = 28

MODEL_FILE = 'config/tu-map-model.json'
MEAN_FUNCTIONS = 14              # cuts per function: the best held-out F1 on X1, finer on ties
HOMOLOG_SPAN = 8                 # function distance for X1 homolog constraints
O0_HOMOLOGS_FILE = 'config/symbols/x1-o0-homologs.json'
O0_HOMOLOG_SPAN = 32             # whole bodies equal to X1's C at -O0: stronger than a fingerprint
O0_CUT_BYTES = 96                # a cut needs bodies this large: at 64-68 bytes X1 functions still collide
POOL_SECTIONS = ('.lit4', '.rodata', '.sdata', '.sbss', '.bss')
STEP_SECTIONS = POOL_SECTIONS + ('.data',)
INVERSION_SPAN = 8               # a datum counts when its users sit this close: not a shared global
INVERSION_WINDOW = 4             # functions read on each side of a gap
INVERSION_DISTANCE = 1024        # farther apart, the two may be objects that data orders unlike text
STEP_TOUCH = 8                   # bytes: the pool continues in the next item
STEP_NEAR = 64
MAX_STEP_SPAN = 64               # functions: farther neighbours in a pool say nothing about one gap
OUT_OF_SCOPE = -99.0
OVERLAYS = (('ov01', 'OV01.OVL', 'annotations/overlays/ov01_annotations.csv'),
            ('ov02', 'OV02.OVL', 'annotations/overlays/ov02_annotations.csv'))
# Checked by map_infer.py generate: no library zone calls game code, except crt0 to main.
# The game zone ends at 0x29f000, not at func_0029F008: the C of the last game object (tu273) is exactly
# the 0x960 bytes of its five functions, so the eight zero bytes before 0x29f008 open the Sony library
# zone (x2-c17 publication, 2026-11-16).
MAIN_ZONES = (
    dict(start=0x120000, end=0x138298, cls='sce_sdk', stem='sce_sdk_low',
         evidence='crt0 and Sony libraries: sce* and libpad strings, no calls into game code, except crt0 to main'),
    # The KORG client ends at 0x13b890, not 0x13b7a0 (zone audit 2026-11-19): func_0013B7A0 indexes a $gp table,
    # func_0013B7E8 is taken by address in func_0013B708 and takes func_0013B7A0, and func_0013B850 uses the
    # client's $gp block and is the pointer stored at 0x69e178. KORG is the only library here built with $gp data.
    dict(start=0x138298, end=0x13b890, cls='middleware', stem='korg_evag', families={'korg': 1},
         evidence='KORG EE client: [KORGEVAG] string, RPC stubs over one client block at 0x69e148, $gp-relative data'),
    dict(start=0x13b890, end=0x194638, cls='middleware', stem='cri_sofdec', families={'cri': 1},
         evidence='CRI ADX and Sofdec: mwPly, SFH and ADXRT strings, no calls into game code'),
    dict(start=0x194638, end=0x29f000, cls='game', stem='game', evidence='from InitializeSystem to the last caller of game code'),
    dict(start=0x29f000, end=0x2a1578, cls='sce_sdk', stem='sce_sdk_high', evidence='Sony libraries by earlier study; no calls into game code'),
    dict(start=0x2a1578, end=0x2b3740, cls='runtime_library', stem='c_runtime', evidence='C runtime by earlier study; no calls into game code'),
)
# Code built without optimization: every function opens a $fp frame, functions are not aligned
# to 8, and C compiled with -O0 reproduces its bytes (reports/x2-compiler-calibration.md).
MAIN_O0_ZONES = ((0x1a75a8, 0x1d5440),)
# Function entries the bytes do not state (no jal, no prologue, no taken address), keyed by image file name.
# cfeFileLogIndentSet (0x24F130) is reached only by the tail jump of cfeFileTempInit; the published C defines it as
# its own function (cfeFileControl.c, upstream 7ffbd0d0), so the function table must start one there.
# msgEntryTestFlags (0x27D750) is reached only by the tail jump of msgWindowTestCurrentFlags (src/main/tu222.c).
KNOWN_STARTS = {'SLUS_208.92': {0x24F130, 0x27D750}}
FP_FROM_SP = 0x03a0f02d           # daddu $fp, $sp, $zero
LOG_ODDS_EDGES = (-3.0, -2.5, -2.0, -1.5, -1.0, -0.5, 0.0, 0.5)

# ---------------------------------------------------------------- image model


class Image:
    """One ELF file with its code range and the data sections it can reference."""

    def __init__(self, path, unit):
        self.unit = unit
        self.o0_zones = ()
        self.path = Path(path)
        self.data = self.path.read_bytes()
        self.sha256 = hashlib.sha256(self.data).hexdigest()
        data = self.data
        shoff = struct.unpack_from('<I', data, 0x20)[0]
        shentsize, shnum, shstrndx = struct.unpack_from('<HHH', data, 0x2e)
        raw = [struct.unpack_from('<10I', data, shoff + i * shentsize) for i in range(shnum)]
        names = raw[shstrndx][4]
        self.sections = {}
        for index, row in enumerate(raw):
            name = data[names + row[0]:data.index(b'\0', names + row[0])].decode('latin1')
            self.sections[name] = dict(index=index, name=name, type=row[1], flags=row[2], va=row[3],
                                       offset=row[4], size=row[5], align=row[8])
        self.overlay = '.text' not in self.sections
        if self.overlay:
            section = self.sections[unit]
            self.code_section = section
            self.text_start, self.section_end = section['va'], section['va'] + section['size']
            self.text_end = self.section_end          # refined by find_overlay_text_end
            self.gp = None
        else:
            section = self.sections['.text']
            self.code_section = section
            self.text_start, self.text_end = section['va'], section['va'] + section['size']
            self.section_end = self.text_end
            reginfo = self.sections['.reginfo']
            self.gp = struct.unpack_from('<I', data, reginfo['offset'] + 20)[0]
        base = section['offset'] - section['va']
        self.base = base
        self.limit_va = section['va'] + section['size']
        self.mapped = [s for s in self.sections.values()
                       if s['va'] and s['flags'] & 2 and s['type'] != 8 and s['size']]

    def word(self, va):
        return struct.unpack_from('<I', self.data, va + self.base)[0]

    def in_o0_zone(self, va):
        return any(low <= va < high for low, high in self.o0_zones)

    def offset_of(self, va):
        for section in self.mapped:
            if section['va'] <= va < section['va'] + section['size']:
                return va - section['va'] + section['offset']
        return None

    def word_any(self, va):
        offset = self.offset_of(va)
        return None if offset is None or offset + 4 > len(self.data) else struct.unpack_from('<I', self.data, offset)[0]

    def section_of(self, va):
        for name, section in self.sections.items():
            if section['va'] and section['flags'] & 2 and section['size'] and section['va'] <= va < section['va'] + section['size']:
                return name
        return None


# ------------------------------------------------------------------ decoding


def branch_kind(word, pc):
    """(kind, target): cond, likely, always, j, jal, jr_ra, jr, jalr, or (None, None)."""
    op = word >> 26
    rs = (word >> 21) & 31
    rt = (word >> 16) & 31
    imm = word & 0xffff
    rel = pc + 4 + ((imm - 0x10000 if imm & 0x8000 else imm) << 2)
    if op == 0x02 or op == 0x03:
        target = ((pc + 4) & 0xf0000000) | ((word & 0x03ffffff) << 2)
        return ('j' if op == 2 else 'jal'), target
    if op == 0:
        funct = word & 63
        if funct == 8:
            return ('jr_ra' if rs == RA else 'jr'), None
        if funct == 9:
            return 'jalr', None
        return None, None
    if op in (0x04, 0x14) and rs == rt:
        return 'always', rel
    if op in (0x04, 0x05, 0x06, 0x07):
        return 'cond', rel
    if op in (0x14, 0x15, 0x16, 0x17):
        return 'likely', rel
    if op == 0x01:
        if rs == 0 and rt in (1, 3):
            return 'always', rel
        if rt in (0, 1, 0x10, 0x11):
            return 'cond', rel
        if rt in (2, 3, 0x12, 0x13):
            return 'likely', rel
        return None, None
    if op in (0x10, 0x11, 0x12) and rs == 8:
        return ('likely' if rt & 2 else 'cond'), rel
    return None, None


def jump_table_at(image, pc, start):
    """(table address, entries) of the `jr` at pc, when its lui/lw pair can be read back."""
    hi = {}
    table = None
    for va in range(max(start, pc - 64), pc, 4):
        word = image.word(va)
        op = word >> 26
        rt = (word >> 16) & 31
        rs = (word >> 21) & 31
        imm = word & 0xffff
        simm = imm - 0x10000 if imm & 0x8000 else imm
        if op == 0x0f:
            hi[rt] = imm << 16
        elif op in (0x23, 0x09):        # lw or addiu off a %hi, directly or through addu
            if rs in hi:
                table = (hi[rs] + simm) & 0xffffffff
        elif op == 0 and (word & 63) in (0x21, 0x2d):
            rd = (word >> 11) & 31
            if rs in hi:
                hi[rd] = hi[rs]
            elif rt in hi:
                hi[rd] = hi[rt]
    if table is None:
        return None, []
    entries = []
    for index in range(2048):
        value = image.word_any(table + 4 * index)
        if value is None or value % 4 or not image.text_start <= value < image.text_end:
            break
        entries.append(value)
    return table, entries


def is_prologue(word):
    """addiu sp,sp,-N: GCC 2.96 never shrink-wraps, so a frame opens at the entry."""
    return word >> 16 == 0x27bd and word & 0x8000 != 0


def follows_function_end(image, va):
    """The code before va ended a function: a terminator and its delay slot, padded or not.

    sched2 can move the frame instruction a few slots past the entry, so a prologue
    only marks an entry when the code before it had already finished.
    """
    if va - 12 < image.text_start:
        return True
    if branch_kind(image.word(va - 8), va - 8)[0] in TERMINATORS + ('jal',):
        return True
    return image.word(va - 4) == PAD and branch_kind(image.word(va - 12), va - 12)[0] in TERMINATORS


def collect_starts(image):
    """Entries the bytes state outright: jal targets, opened frames, taken addresses in data."""
    starts = {image.text_start} | KNOWN_STARTS.get(image.path.name, set())
    tables = {}
    for va in range(image.text_start, image.text_end, 4):
        word = image.word(va)
        if word >> 26 == 3:
            target = ((va + 4) & 0xf0000000) | ((word & 0x03ffffff) << 2)
            if image.text_start <= target < image.text_end and (target % 8 == 0 or image.in_o0_zone(target)):
                starts.add(target)
        elif (va % 8 == 0 or image.in_o0_zone(va)) and is_prologue(word) and follows_function_end(image, va):
            starts.add(va)
        elif word & 0xfc1fffff == 8 and (word >> 21) & 31 != RA:
            table, entries = jump_table_at(image, va, max(image.text_start, va - 64))
            if entries:
                tables[table] = len(entries)
    covered = set()
    for table, count in tables.items():
        covered.update(range(table, table + 4 * count, 4))
    for section in image.sections.values():
        if not section['flags'] & 2 or section['flags'] & 4 or section['type'] == 8 or not section['size']:
            continue
        if section is image.code_section:
            continue
        offset, size = section['offset'], section['size']
        for position in range(0, size - size % 4, 4):
            value = struct.unpack_from('<I', image.data, offset + position)[0]
            if value % 8 or not image.text_start <= value < image.text_end:
                continue
            if section['va'] + position in covered:
                continue
            if image.word(value - 4) == PAD or branch_kind(image.word(value - 8), value - 8)[0] in TERMINATORS:
                starts.add(value)
    return starts, tables


TERMINATORS = ('jr_ra', 'jr', 'j', 'always')


def sweep(image, start, end, starts):
    """Linear sweep: a function ends at a terminator past the reach of its branches.

    A known start always ends the function before it, so one wrong branch reach
    cannot swallow more than the code up to the next known entry.
    """
    ordered = sorted(starts)
    functions = []
    pc = start
    while pc < end:
        if image.word(pc) == PAD:
            pc += 4
            continue
        first = pc
        index = bisect.bisect_right(ordered, first)
        limit = ordered[index] if index < len(ordered) and ordered[index] < end else end
        reach = pc
        while pc < limit:
            kind, target = branch_kind(image.word(pc), pc)
            if kind in ('cond', 'likely', 'always', 'j') and pc < target < limit and target > reach:
                reach = target
            elif kind == 'jr':
                for value in jump_table_at(image, pc, first)[1]:
                    if reach < value < limit:
                        reach = value
            if kind in TERMINATORS and pc + 8 > reach:
                pc += 8
                break
            pc += 4
        pc = min(pc, limit)
        functions.append(dict(va=first, end=pc))
    return functions


def find_functions(image):
    """Function list for the code range: known entries first, then the sweep between them."""
    starts, tables = collect_starts(image)
    functions = sweep(image, image.text_start, image.text_end, starts)
    return merge_interior_blocks(image, functions), starts


def merge_interior_blocks(image, functions):
    """Inside an -O0 zone every function opens a $fp frame, so any other start is one of its blocks.

    Switch cases are what the sweep splits here: their tables use the -O0 load sequence.
    """
    merged = []
    for function in functions:
        va = function['va']
        opens_frame = any(image.word(at) == FP_FROM_SP for at in range(va, min(va + 64, function['end']), 4))
        if merged and image.in_o0_zone(va) and not opens_frame:
            merged[-1]['end'] = function['end']
        else:
            merged.append(function)
    return merged


# -------------------------------------------------------------- reference scan

LOAD_GPR = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x37, 0x1a, 0x1b, 0x1e}
LOAD_FPR = {0x31, 0x35, 0x36}
STORES = {0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x3f, 0x1f, 0x39, 0x3d, 0x3e}
IALU_DEST_RT = {0x08, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x18}
CALLER_SAVED = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 31}


def written_gprs(word):
    """GPRs an EE instruction may write (an over-approximation: unknown forms count as none only
    when they cannot name a GPR destination)."""
    op = word >> 26
    rs = (word >> 21) & 31
    rt = (word >> 16) & 31
    rd = (word >> 11) & 31
    if op in (0x00, 0x1c):
        return {rd} - {0}
    if op == 0x01:
        return {31} if rt in (0x10, 0x11, 0x12, 0x13) else set()
    if op == 0x03:
        return {31}
    if 0x08 <= op <= 0x0f or op in (0x18, 0x19) or op in LOAD_GPR:
        return {rt} - {0}
    if op in (0x10, 0x11, 0x12) and rs in (0, 1, 2):
        return {rt} - {0}
    return set()


def single_his(image, function):
    """Registers whose only write in the function is one `lui`: {register: (lui_va, value)}.

    The epilogue's restore of a callee-saved register from its prologue slot does not count.

    Such a register holds that %hi wherever it is read, across loop heads and other joins
    (a long-lived callee-saved %hi base, e.g. `lui $30,%hi(table)` before a loop whose
    `%lo(table)($30)` sits behind the loop label)."""
    callee_saved = set(range(16, 24)) | {30, 31}
    saves = set()
    for va in range(function['va'], function['end'], 4):
        word = image.word(va)
        if word >> 26 in STORES and (word >> 21) & 31 == 29 and (word >> 16) & 31 in callee_saved:
            saves.add(((word >> 16) & 31, word & 0xffff))
    writes = collections.Counter()
    his = {}
    for va in range(function['va'], function['end'], 4):
        word = image.word(va)
        if (word >> 26 in LOAD_GPR and (word >> 21) & 31 == 29
                and ((word >> 16) & 31, word & 0xffff) in saves):
            continue  # the epilogue restoring the caller's value from its prologue slot
        for register in written_gprs(word):
            writes[register] += 1
        if word >> 26 == 0x0f:
            his[(word >> 16) & 31] = (va, ((word & 0xffff) << 16) & 0xffffffff)
    return {r: his[r] for r in his if writes[r] == 1}


def scan_refs(image, function):
    """%hi/%lo and $gp references of one function: (insn_va, target, kind, access).

    A forward sweep that keeps the %hi value of each register and forgets it at
    branch targets, delay slots of calls and overwrites. It sees less than a full
    dataflow, and everything it reports is a pair the code really forms. A register
    written only by one `lui` keeps its %hi across joins; those pairs are reported as
    'hi_lo_join' (data attribution only, like 'hi_lo_indexed').
    """
    first, end = function['va'], function['end']
    singles = single_his(image, function)
    joined = {}
    join = set()
    for va in range(first, end, 4):
        kind, target = branch_kind(image.word(va), va)
        if kind in ('cond', 'likely', 'always', 'j') and first <= target < end:
            join.add(target)
    state = {}
    indexed = {}
    out = []
    pending_clobber = False
    for va in range(first, end, 4):
        word = image.word(va)
        if va in join:
            state = {}
            indexed = {}
            joined = {r: value for r, (lui_va, value) in singles.items() if lui_va < va}
        op = word >> 26
        rs = (word >> 21) & 31
        rt = (word >> 16) & 31
        rd = (word >> 11) & 31
        imm = word & 0xffff
        simm = imm - 0x10000 if imm & 0x8000 else imm
        if op in (0x09, 0x19):
            if rt != GP:
                if rs == GP and image.gp is not None:
                    out.append((va, (image.gp + simm) & 0xffffffff, 'gp', 'addr'))
                elif rs in state:
                    out.append((va, (state[rs] + simm) & 0xffffffff, 'hi_lo', 'addr'))
                elif rs in joined:
                    out.append((va, (joined[rs] + simm) & 0xffffffff, 'hi_lo_join', 'addr'))
            state.pop(rt, None)
            indexed.pop(rt, None)
        elif op == 0x0f:
            state[rt] = (imm << 16) & 0xffffffff
            indexed.pop(rt, None)
        elif op in LOAD_GPR or op in LOAD_FPR or op in STORES:
            access = 'float' if op in LOAD_FPR else 'mem'
            if rs == GP and image.gp is not None:
                out.append((va, (image.gp + simm) & 0xffffffff, 'gp', access))
            elif rs in state:
                out.append((va, (state[rs] + simm) & 0xffffffff, 'hi_lo', access))
            elif rs in indexed:
                out.append((va, (indexed[rs] + simm) & 0xffffffff, 'hi_lo_indexed', access))
            elif rs in joined:
                out.append((va, (joined[rs] + simm) & 0xffffffff, 'hi_lo_join', access))
            if op in LOAD_GPR:
                state.pop(rt, None)
                indexed.pop(rt, None)
        elif op in IALU_DEST_RT:
            state.pop(rt, None)
            indexed.pop(rt, None)
        elif op == 0:
            if rd:
                # `lui r,%hi(table); addu r,r,index; lw x,%lo(table)(r)`: the indexed access of a
                # jump table or array keeps the %hi of the one operand that holds it.
                funct = word & 0x3f
                based = [state[r] for r in (rs, rt) if r in state]
                if funct in (0x21, 0x2d) and len(based) == 1 and rs != rt:
                    indexed[rd] = based[0]
                else:
                    indexed.pop(rd, None)
                state.pop(rd, None)
        elif op == 0x1c:
            state.pop(rd, None)
            indexed.pop(rd, None)
        elif op in (0x10, 0x11, 0x12) and rs in (0, 1, 2):
            state.pop(rt, None)
            indexed.pop(rt, None)
        if pending_clobber:
            for register in CALLER_SAVED:
                state.pop(register, None)
                indexed.pop(register, None)
                joined.pop(register, None)
            pending_clobber = False
        kind, _ = branch_kind(word, va)
        if kind in ('jal', 'jalr'):
            pending_clobber = True
    return out


def scan_calls(image, function):
    """Code addresses this function transfers to: jal, and j outside its own body."""
    first, end = function['va'], function['end']
    calls = []
    for va in range(first, end, 4):
        kind, target = branch_kind(image.word(va), va)
        if kind == 'jal' or (kind == 'j' and not first <= target < end):
            calls.append(target)
    return calls


def string_at(image, va):
    """The NUL-terminated text starting at va, or None: ASCII plus valid EUC pairs, two chars or more."""
    offset = image.offset_of(va)
    if offset is None or offset == 0 or image.data[offset - 1] != 0:
        return None
    position = offset
    ascii_count = wide_count = 0
    size = len(image.data)
    while position < size:
        byte = image.data[position]
        if byte == 0:
            break
        if 0x20 <= byte < 0x7f or byte in (9, 10, 13):
            ascii_count += 1
            position += 1
        elif 0xa1 <= byte <= 0xfe and position + 1 < size and 0xa1 <= image.data[position + 1] <= 0xfe:
            wide_count += 1
            position += 2
        else:
            return None
        if position - offset > 4096:
            return None
    if ascii_count + wide_count < 2:
        return None
    return image.data[offset:position]


def is_source_file(text):
    return text.endswith(b'.c') and (b'/' in text or b'\\' in text or text.count(b'.') >= 1) and b' ' not in text


def text_at(image, va):
    """Real text at va, or None: NUL-preceded printable ASCII/EUC with four or more
    characters, an EUC character, or a % format. Shorter runs are mostly pointers and
    small integers (0x00746C40 reads "@lt")."""
    offset = image.offset_of(va)
    if offset is None or offset == 0 or image.data[offset - 1] != 0:
        return None
    position = offset
    count = wide = 0
    size = len(image.data)
    while position < size and image.data[position] != 0:
        byte = image.data[position]
        if 0x20 <= byte < 0x7f or byte in (9, 10, 11, 12, 13):
            position += 1
        elif 0xa1 <= byte <= 0xfe and position + 1 < size and 0xa1 <= image.data[position + 1] <= 0xfe:
            position += 2
            wide += 1
        else:
            return None
        count += 1
        if position - offset > 4096:
            return None
    text = image.data[offset:position]
    if count >= 4 or wide or (b'%' in text and count >= 2):
        return text
    return None


# ------------------------------------------------------------- gap features

SPAN_BUCKETS = (2, 4, 8, 16, 32, 64)
COHESION_WINDOWS = (2, 4, 8, 16)
FEATURE_NAMES = (
    [f'cohesion_{w}' for w in COHESION_WINDOWS]
    + [f'calls_back_{s}' for s in SPAN_BUCKETS]
    + [f'calls_fwd_{s}' for s in SPAN_BUCKETS]
    + [f'data_{s}' for s in SPAN_BUCKETS]
    + [f'cohesion_calls_{w}' for w in (4, 8)]
    + [f'cohesion_data_{w}' for w in (4, 8)]
    + [f'depth_{w}' for w in (4, 8, 16)]
    + ['string_tight', 'string_loose']
    + [f'pool_{kind}_{section[1:]}' for section in STEP_SECTIONS for kind in ('touch', 'near', 'far', 'back')]
    + ['order_4', 'order_8', 'left_far_called', 'right_far_called', 'left_uncalled', 'right_uncalled',
       'left_size', 'right_size', 'bias']
)


class Unit:
    """Functions of one image with what they reference and everything derived per gap."""

    def __init__(self, image):
        self.image = image
        self.functions, self.starts = find_functions(image)
        self.vas = [f['va'] for f in self.functions]
        self.index = {va: i for i, va in enumerate(self.vas)}
        self.refs = [scan_refs(image, f) for f in self.functions]
        self.calls = [scan_calls(image, f) for f in self.functions]
        self._section_users = {}
        self._link()

    def is_data(self, target):
        image = self.image
        if image.text_start <= target < image.text_end:
            return False
        return image.section_of(target) is not None

    def _link(self):
        count = len(self.functions)
        self.users = collections.defaultdict(set)
        # Indexed references (jump tables, arrays) feed data attribution only; the boundary
        # model was calibrated without them, so the text cuts stay where they were.
        self.indexed_users = collections.defaultdict(set)
        self.tokens = [collections.Counter() for _ in range(count)]
        self.points = collections.defaultdict(lambda: [[] for _ in range(count)])
        for i in range(count):
            for _, target, kind, _ in self.refs[i]:
                if not self.is_data(target):
                    continue
                if kind in ('hi_lo_indexed', 'hi_lo_join'):
                    self.indexed_users[target].add(i)
                    continue
                self.users[target].add(i)
                self.tokens[i]['d', target] += 1
                self.tokens[i]['p', target >> 6] += 1
                self.tokens[i]['q', target >> 10] += 1
                self.points[self.image.section_of(target)][i].append(target)
            for target in self.calls[i]:
                self.tokens[i]['c', target] += 1
        self.callers = collections.defaultdict(set)
        self.far_callers = [0] * count
        for i, targets in enumerate(self.calls):
            for target in targets:
                j = self.index.get(target)
                if j is not None and j != i:
                    self.callers[j].add(i)
        for j, who in self.callers.items():
            self.far_callers[j] = sum(1 for i in who if abs(i - j) > 64)
        self.strings = {}
        for target in self.users:
            value = string_at(self.image, target)
            if value is not None:
                self.strings[target] = value
        # A long path or message used by several functions belongs to one object, however
        # far apart they sit. A short string may be a named global array: only a near span counts.
        self.string_spans = []
        for target, value in self.strings.items():
            who = self.users[target]
            if len(who) < 2:
                continue
            low, high = min(who), max(who)
            if is_source_file(value) or (len(value) >= 8 and high - low <= 32):
                self.string_spans.append((low, high))
        # A literal is emitted once per object and never shared across objects (GCC 2.96 and
        # ld merge nothing), so every user of one real text in .rodata or .sdata, at any
        # distance, sits in one object. X1 main: 0 of 83 such texts cross an object edge
        # (.sdata globals do cross, 52 times, which is why the text test is strict).
        self.text_spans = []
        for target, who in self.users.items():
            if len(who) < 2 or self.image.section_of(target) not in ('.rodata', '.sdata'):
                continue
            value = text_at(self.image, target)
            if value is not None:
                self.text_spans.append((min(who), max(who), target, value))

    def hard_no_cut(self):
        """Gaps a string crosses (X1: no string used by two objects, 0 of 691) or an inversion forbids."""
        blocked = [False] * max(0, len(self.functions) - 1)
        for low, high in self.string_spans:
            for gap in range(low, high):
                blocked[gap] = True
        for gap in self.pool_inversions():
            blocked[gap] = True
        return blocked

    def section_users(self, section):
        """function index -> sorted addresses of one data section it references."""
        if section not in self._section_users:
            per = collections.defaultdict(set)
            for target, who in self.users.items():
                if self.image.section_of(target) == section:
                    for i in who:
                        per[i].add(target)
            self._section_users[section] = {i: sorted(values) for i, values in per.items()}
        return self._section_users[section]

    def pool_steps(self, section):
        """(i, j, step in bytes, last of i, first of j) between neighbouring users of one section."""
        per = self.section_users(section)
        ordered = sorted(per)
        return [(i, j, per[j][0] - per[i][-1], per[i][-1], per[j][0]) for i, j in zip(ordered, ordered[1:])]

    def pool_inversions(self):
        """Gaps with a local datum on the left above one on the right.

        Each object owns one pool per section, in link order, so a later object cannot use
        lower addresses. On X1 main no such gap is an object edge (0 of 279).
        """
        count = len(self.functions)
        blocked = set()
        for section in POOL_SECTIONS:
            high, low = {}, {}
            for target, who in self.users.items():
                if self.image.section_of(target) != section or max(who) - min(who) > INVERSION_SPAN:
                    continue
                for i in who:
                    high[i] = max(high.get(i, target), target)
                    low[i] = min(low.get(i, target), target)
            for gap in range(count - 1):
                left = [high[i] for i in range(max(0, gap - INVERSION_WINDOW + 1), gap + 1) if i in high]
                right = [low[j] for j in range(gap + 1, min(count, gap + 1 + INVERSION_WINDOW)) if j in low]
                if left and right and 0 < max(left) - min(right) <= INVERSION_DISTANCE:
                    blocked.add(gap)
        return blocked

    def duplicate_string_intervals(self):
        """Gap ranges between two copies of one text: cc1 pools strings per object, ld never merges them."""
        if '.rodata' not in self.image.sections:
            return []
        by_text = collections.defaultdict(list)
        for target, value in self.strings.items():
            if len(value) >= 4 and self.image.section_of(target) == '.rodata':
                by_text[value].append(target)
        intervals = []
        for addresses in by_text.values():
            addresses.sort()
            for a, b in zip(addresses, addresses[1:]):
                left, right = self.users[a], self.users[b]
                if max(left) < min(right):
                    low, high = max(left), min(right)
                elif max(right) < min(left):
                    low, high = max(right), min(left)
                else:
                    continue
                intervals.append((low, high - 1, 'dup_string', f'copies at {a:#x} and {b:#x}'))
        return intervals

    def lit4_step_intervals(self):
        """Gap ranges where .lit4 does not continue: each li.s takes the next word of its object's pool."""
        return [(i, j - 1, 'lit4_step', f'.lit4 {a:#x} -> {b:#x}')
                for i, j, step, a, b in self.pool_steps('.lit4') if step < 0 or step > STEP_TOUCH]

    def rodata_padding_intervals(self):
        """Gap ranges around a string followed by 8 zero bytes or more, up to a 16-aligned item."""
        image = self.image
        if '.rodata' not in image.sections:
            return []
        items = sorted(target for target in self.users if image.section_of(target) == '.rodata')
        intervals = []
        for a, b in zip(items, items[1:]):
            value = self.strings.get(a)
            if value is None or b % 16:
                continue
            end = a + len(value) + 1
            if b - end < 8 or any(image.data[image.offset_of(end):image.offset_of(b)]):
                continue
            left, right = self.users[a], self.users[b]
            if max(left) < min(right):
                intervals.append((max(left), min(right) - 1, 'rodata_pad', f'.rodata {a:#x} -> {b:#x}'))
        return intervals

    def source_files(self):
        """function index -> source path, for the functions that assert with __FILE__."""
        files = {}
        for target, value in self.strings.items():
            if is_source_file(value):
                for i in self.users[target]:
                    files.setdefault(i, set()).add(value)
        return files

    def must_cut_intervals(self):
        """(low, high, kind, note) gap ranges that hold a cut: neighbours naming different files."""
        files = self.source_files()
        ordered = sorted(i for i, names in files.items() if len(names) == 1)
        intervals = []
        for a, b in zip(ordered, ordered[1:]):
            if files[a] != files[b]:
                note = f'{next(iter(files[a])).decode("latin1")} -> {next(iter(files[b])).decode("latin1")}'
                intervals.append((a, b - 1, 'file_path', note))
        return intervals

    def rodata_order_breaks(self):
        """Gap ranges where strings jump in .rodata between two runs that touch in .text.

        Objects keep one order inside a section, so this jump needs an object edge. Needs a
        real .rodata section, and runs of two strings or more to stay off stray references.
        """
        image = self.image
        if '.rodata' not in image.sections:
            return []
        rows = sorted((target, min(self.users[target])) for target in self.strings
                      if image.section_of(target) == '.rodata')
        runs = []
        for target, i in rows:
            if runs and runs[-1]['high'] - 8 <= i <= runs[-1]['high'] + 32:
                run = runs[-1]
                run['high'] = max(run['high'], i)
                run['low'] = min(run['low'], i)
                run['count'] += 1
            else:
                runs.append(dict(low=i, high=i, count=1, rank=len(runs), head=target))
        runs = sorted((run for run in runs if run['count'] >= 2), key=lambda run: run['low'])
        intervals = []
        for a, b in zip(runs, runs[1:]):
            if a['high'] < b['low'] and b['rank'] != a['rank'] + 1:
                intervals.append((a['high'], b['low'] - 1, 'rodata_order', f'.rodata {a["head"]:#x} -> {b["head"]:#x}'))
        return intervals

    def optimization_edges(self):
        """Gaps at the edge of an -O0 zone: one object has one compile contract."""
        edges = []
        for low, high in self.image.o0_zones:
            for va, side in ((low, 'start'), (high, 'end')):
                position = bisect.bisect_left(self.vas, va)
                if 0 < position < len(self.vas):
                    edges.append((position - 1, position - 1, 'opt_level', f'{side} of the -O0 zone {low:#x}-{high:#x}'))
        return edges

    def forced_cuts(self):
        return (self.optimization_edges() + self.must_cut_intervals() + self.rodata_order_breaks()
                + self.duplicate_string_intervals() + self.lit4_step_intervals() + self.rodata_padding_intervals())

    def features(self):
        count = len(self.functions)
        gaps = count - 1
        document_frequency = collections.Counter()
        for tokens in self.tokens:
            document_frequency.update(tokens.keys())
        idf = {key: math.log((count + 1) / (value + 0.5)) for key, value in document_frequency.items()}
        rows = [[0.0] * len(FEATURE_NAMES) for _ in range(gaps)]
        slot = {name: position for position, name in enumerate(FEATURE_NAMES)}
        for window in COHESION_WINDOWS:
            column = slot[f'cohesion_{window}']
            for gap in range(gaps):
                left = weighted_window(self.tokens, idf, max(0, gap - window + 1), gap + 1, gap)
                right = weighted_window(self.tokens, idf, gap + 1, min(count, gap + 1 + window), gap + 1)
                rows[gap][column] = cosine(left, right)
        for name, keep in (('cohesion_calls', 'c'), ('cohesion_data', 'dpq')):
            for window in (4, 8):
                column = slot[f'{name}_{window}']
                for gap in range(gaps):
                    left = weighted_window(self.tokens, idf, max(0, gap - window + 1), gap + 1, gap, keep)
                    right = weighted_window(self.tokens, idf, gap + 1, min(count, gap + 1 + window), gap + 1, keep)
                    rows[gap][column] = cosine(left, right)
        for window in (4, 8, 16):
            source = slot[f'cohesion_{window}']
            column = slot[f'depth_{window}']
            for gap in range(gaps):
                here = rows[gap][source]
                left = max(rows[j][source] for j in range(max(0, gap - 4), gap + 1))
                right = max(rows[j][source] for j in range(gap, min(gaps, gap + 5)))
                rows[gap][column] = (left - here) + (right - here)
        self._string_adjacency(rows, slot)
        self._pool_step_features(rows, slot)
        for caller, targets in enumerate(self.calls):
            for target in targets:
                callee = self.index.get(target)
                if callee is None or callee == caller:
                    continue
                low, high = min(caller, callee), max(caller, callee)
                name = 'calls_back' if callee < caller else 'calls_fwd'
                for bucket in SPAN_BUCKETS:
                    if high - low <= bucket:
                        column = slot[f'{name}_{bucket}']
                        for gap in range(low, high):
                            rows[gap][column] += 1
        for who in self.users.values():
            if len(who) < 2:
                continue
            low, high = min(who), max(who)
            for bucket in SPAN_BUCKETS:
                if high - low <= bucket:
                    column = slot[f'data_{bucket}']
                    for gap in range(low, high):
                        rows[gap][column] += 1
        for name in [f'{kind}_{bucket}' for kind in ('calls_back', 'calls_fwd', 'data') for bucket in SPAN_BUCKETS]:
            column = slot[name]
            for row in rows:
                row[column] = math.log1p(row[column])
        for window, name in ((4, 'order_4'), (8, 'order_8')):
            column = slot[name]
            for gap in range(gaps):
                rows[gap][column] = self._order_inversion(gap, window)
        for gap in range(gaps):
            row = rows[gap]
            row[slot['left_far_called']] = min(self.far_callers[gap], 8) / 8
            row[slot['right_far_called']] = min(self.far_callers[gap + 1], 8) / 8
            row[slot['left_uncalled']] = float(gap not in self.callers)
            row[slot['right_uncalled']] = float(gap + 1 not in self.callers)
            left, right = self.functions[gap], self.functions[gap + 1]
            row[slot['left_size']] = math.log(max(left['end'] - left['va'], 4)) / 10
            row[slot['right_size']] = math.log(max(right['end'] - right['va'], 4)) / 10
            row[slot['bias']] = 1.0
        return rows

    def _pool_step_features(self, rows, slot):
        """How each pool continues between neighbouring users, spread over the gaps between them."""
        for section in STEP_SECTIONS:
            for i, j, step, _, _ in self.pool_steps(section):
                if j - i > MAX_STEP_SPAN:
                    continue
                kind = 'back' if step < 0 else 'touch' if step <= STEP_TOUCH else 'near' if step <= STEP_NEAR else 'far'
                column = slot[f'pool_{kind}_{section[1:]}']
                weight = 1.0 / (j - i)
                for gap in range(i, j):
                    rows[gap][column] = max(rows[gap][column], weight)

    def _string_adjacency(self, rows, slot):
        """Consecutive strings: packed ones sit in one object, ones with data between may not."""
        ordered = sorted(self.strings)
        for a, b in zip(ordered, ordered[1:]):
            if self.image.section_of(a) != self.image.section_of(b):
                continue
            end = a + len(self.strings[a]) + 1
            tight = b == end or b == (end + 7) & ~7
            column = slot['string_tight' if tight else 'string_loose']
            who_a, who_b = self.users[a], self.users[b]
            low, high = min(who_a | who_b), max(who_a | who_b)
            if max(who_a) < min(who_b):
                low, high = max(who_a), min(who_b)
            if high - low > 64:
                continue
            for gap in range(low, high):
                rows[gap][column] = 1.0

    def _order_inversion(self, gap, window):
        """Share of data-reference pairs across the gap out of link order; 0.5 without data."""
        total = weight = 0.0
        count = len(self.functions)
        for per_function in self.points.values():
            left = [v for j in range(max(0, gap - window + 1), gap + 1) for v in per_function[j]]
            if not left:
                continue
            right = sorted(v for j in range(gap + 1, min(count, gap + 1 + window)) for v in per_function[j])
            if not right:
                continue
            above = sum(len(right) - bisect.bisect_right(right, value) for value in left)
            pairs = len(left) * len(right)
            total += above
            weight += pairs
        return total / weight if weight else 0.5


def weighted_window(tokens, idf, low, high, anchor, keep=None):
    window = collections.Counter()
    for j in range(low, high):
        decay = 0.8 ** abs(j - anchor)
        for key, value in tokens[j].items():
            if keep is None or key[0] in keep:
                window[key] += decay * value * idf[key]
    return window


def cosine(left, right):
    if not left or not right:
        return 0.0
    if len(left) > len(right):
        left, right = right, left
    dot = sum(value * right.get(key, 0.0) for key, value in left.items())
    norm = math.sqrt(sum(v * v for v in left.values()) * sum(v * v for v in right.values()))
    return dot / norm if norm else 0.0


# ------------------------------------------------------------- model and decode


def train(rows, labels, epochs=400, rate=0.3, l2=1e-3):
    """Logistic regression, numpy only here; the weights land in a plain JSON file."""
    import numpy
    x = numpy.array(rows, dtype=float)
    y = numpy.array(labels, dtype=float)
    mean = x.mean(axis=0)
    std = x.std(axis=0)
    std[std == 0] = 1.0
    mean[-1] = 0.0
    std[-1] = 1.0
    z = (x - mean) / std
    w = numpy.zeros(x.shape[1])
    positive = max(y.sum(), 1.0)
    balance = numpy.where(y > 0, (len(y) - positive) / positive, 1.0)
    balance /= balance.mean()
    for _ in range(epochs):
        p = 1 / (1 + numpy.exp(-numpy.clip(z @ w, -30, 30)))
        gradient = z.T @ ((p - y) * balance) / len(y) + l2 * w
        w -= rate * gradient
    return dict(names=FEATURE_NAMES, weights=w.tolist(), mean=mean.tolist(), std=std.tolist(),
                prior=float(y.mean()), balanced=True)


def score(model, rows):
    """Log-odds of a cut, corrected for the class balancing used in training."""
    shift = math.log(model['prior'] / (1 - model['prior']))
    out = []
    for row in rows:
        value = sum(w * (x - m) / s for w, x, m, s in zip(model['weights'], row, model['mean'], model['std']))
        out.append(value + shift)
    return out


def bucket_of(value):
    position = bisect.bisect_right(LOG_ODDS_EDGES, value)
    return f'band_{position:02d}'


def segment(log_odds, blocked, forced, target, radius=2):
    """Cuts as {gap: [(kind, note)...]}: forced ranges first, then the best gaps up to target.

    A forced range takes its best unblocked gap. Scored cuts keep radius gaps away from
    every other cut and never land on a blocked gap.
    """
    gaps = len(log_odds)
    cuts = {}
    for low, high, kind, note in forced:
        options = [g for g in range(max(low, 0), min(high, gaps - 1) + 1) if log_odds[g] > OUT_OF_SCOPE]
        if not options:
            continue
        taken = [g for g in options if g in cuts]
        if taken:
            cuts[taken[0]].append((kind, note))
            continue
        free = [g for g in options if not blocked[g]] or options
        cuts[max(free, key=lambda g: log_odds[g])] = [(kind, note)]
    ranked = sorted((g for g in range(gaps) if log_odds[g] > OUT_OF_SCOPE and not blocked[g]),
                    key=lambda g: -log_odds[g])
    for gap in ranked:
        if len(cuts) >= target:
            break
        if gap not in cuts and all(abs(gap - other) > radius for other in cuts):
            cuts[gap] = []
    return cuts


def boundary_metrics(cuts, truth, tolerance=0):
    truth = sorted(truth)
    used = set()
    hits = 0
    for cut in sorted(cuts):
        for candidate in range(cut - tolerance, cut + tolerance + 1):
            if candidate in truth and candidate not in used:
                used.add(candidate)
                hits += 1
                break
    precision = hits / len(cuts) if cuts else 0.0
    recall = hits / len(truth) if truth else 0.0
    f1 = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
    return dict(cuts=len(cuts), truth=len(truth), precision=round(precision, 3), recall=round(recall, 3), f1=round(f1, 3))


# ------------------------------------------------------- text end of an overlay


INVALID_OPCODES = {0x13, 0x1d, 0x30, 0x32, 0x34, 0x35, 0x38, 0x3a, 0x3b, 0x3c, 0x3d}
SPECIAL_ALU = {0x0a, 0x0b, 0x14, 0x16, 0x17, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2a, 0x2b,
               0x2c, 0x2d, 0x2e, 0x2f, 0x04, 0x06, 0x07}
SPECIAL_SHIFT = {0x00, 0x02, 0x03, 0x38, 0x3a, 0x3b, 0x3c, 0x3e, 0x3f}
SPECIAL_OTHER = {0x08, 0x09, 0x0c, 0x0d, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1a, 0x1b, 0x28, 0x29}


def plausible_instruction(word, pc, image):
    """False for words real code never holds: data bytes fail this almost always."""
    if word == 0:
        return True
    op = word >> 26
    rs = (word >> 21) & 31
    rt = (word >> 16) & 31
    rd = (word >> 11) & 31
    sa = (word >> 6) & 31
    if op in INVALID_OPCODES:
        return False
    if op == 0:
        funct = word & 63
        if funct in SPECIAL_SHIFT:
            return rs == 0 and rd != 0
        if funct in SPECIAL_ALU:
            return sa == 0 and rd != 0
        return funct in SPECIAL_OTHER
    if op == 1:
        return rt in (0, 1, 2, 3, 0x10, 0x11, 0x12, 0x13, 0x18, 0x19)
    if op in (2, 3):
        target = (word & 0x03ffffff) << 2
        return 0x100000 <= target < 0x2000000 and target % 4 == 0
    kind, target = branch_kind(word, pc)
    if kind in ('cond', 'likely', 'always'):
        return image.text_start <= target < image.section_end
    if op == 0x0f:
        return rs == 0 and rt != 0
    if op in (0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x18, 0x19) or op in LOAD_GPR:
        return rt != 0
    return True


def find_text_end(image):
    """End of code in an overlay section, whose headers merge text and data.

    Walk functions in order. Stop at the first one holding a word no code holds, or at the
    lowest address a load or store has named: code never loads from code.
    """
    lowest = image.section_end
    pc = image.text_start
    last_end = pc
    image.text_end = image.section_end
    while pc < lowest:
        if image.word(pc) == PAD:
            pc += 4
            continue
        function = next_function(image, pc, lowest)
        if not all(plausible_instruction(image.word(va), va, image) for va in range(function['va'], function['end'], 4)):
            break
        for _, target, kind, access in scan_refs(image, function):
            if kind not in ('hi_lo_indexed', 'hi_lo_join') and access != 'addr' and function['end'] <= target < lowest:
                lowest = target
        last_end = min(function['end'], lowest)
        pc = function['end']
    return last_end


def next_function(image, first, limit):
    reach = pc = first
    while pc < limit:
        kind, target = branch_kind(image.word(pc), pc)
        if kind in ('cond', 'likely', 'always', 'j') and pc < target < limit and target > reach:
            reach = target
        if kind in TERMINATORS and pc + 8 > reach:
            return dict(va=first, end=min(pc + 8, limit))
        pc += 4
    return dict(va=first, end=limit)


# ------------------------------------------------------------------- X1 truth


def x1_truth():
    """The X1 units with their symbol-derived TUs: the only ground truth this project has."""
    base = X1_ROOT / '.work/tu-migration'
    main_map = json.loads((base / 'map-main/tu-map-main.json').read_text())
    overlays = json.loads((base / 'map-ovl/tu-map-ovl.json').read_text())['overlays']
    out = {}
    image = Image(X1_ROOT / 'game/extracted-iso/SLUS_204.69', 'main')
    owner = {}
    for tu in main_map['tus']:
        for function in tu['functions']:
            owner[int(function['va'], 16)] = (tu['ordinal'], tu['classification']['cls'])
    out['main'] = dict(image=image, owner=owner)
    for unit, record in overlays.items():
        image = Image(X1_ROOT / record['path'], unit)
        image.true_text_end = int(record['layout']['text_end'], 16)
        image.text_end = image.true_text_end
        owner = {int(f['va'], 16): (tu['ordinal'], 'game') for tu in record['tus'] for f in tu['functions']}
        out[unit] = dict(image=image, owner=owner)
    return out


def score_functions():
    rows = {}
    for unit, record in x1_truth().items():
        image = record['image']
        functions, _ = find_functions(image)
        found = {f['va'] for f in functions}
        game = {va for va, (_, cls) in record['owner'].items() if cls == 'game'}
        low, high = min(game), max(game)
        inside = {va for va in found if low <= va <= high}
        hits = len(game & inside)
        rows[unit] = dict(truth=len(game), found=len(inside), recall=round(hits / len(game), 4),
                          precision=round(hits / len(inside), 4))
        if hasattr(image, 'true_text_end'):
            rows[unit]['text_end_true'] = hex(image.true_text_end)
            rows[unit]['text_end_found'] = hex(find_text_end(image))
            image.text_end = image.true_text_end
    return rows


def labelled_gaps(unit, owner):
    """(gap, is_boundary) for gaps with game code on both sides, by the true owner of each start."""
    starts = sorted(owner)

    def who(va):
        position = bisect.bisect_right(starts, va) - 1
        return owner[starts[position]] if position >= 0 else (None, None)
    rows = []
    for gap in range(len(unit.vas) - 1):
        left, right = who(unit.vas[gap]), who(unit.vas[gap + 1])
        if left[1] == 'game' and right[1] == 'game':
            rows.append((gap, int(left[0] != right[0])))
    return rows


def calibrate(mean_functions=MEAN_FUNCTIONS):
    """Leave-one-unit-out on X1: every number here comes from a unit the model did not train on."""
    data = {}
    for name, record in x1_truth().items():
        unit = Unit(record['image'])
        labelled = labelled_gaps(unit, record['owner'])
        data[name] = dict(unit=unit, rows=unit.features(), gaps=[g for g, _ in labelled],
                          labels=[v for _, v in labelled], blocked=unit.hard_no_cut())
    folds = {}
    buckets = collections.defaultdict(lambda: [0, 0])
    forced_hits = collections.Counter()
    pooled = collections.Counter()
    for held, record in data.items():
        rows, labels = [], []
        for name, other in data.items():
            if name != held:
                rows += [other['rows'][g] for g in other['gaps']]
                labels += other['labels']
        model = train(rows, labels)
        in_scope = set(record['gaps'])
        odds = score(model, record['rows'])
        log_odds = [value if gap in in_scope else OUT_OF_SCOPE for gap, value in enumerate(odds)]
        truth = {g for g, v in zip(record['gaps'], record['labels']) if v}
        unit = record['unit']
        forced = unit.forced_cuts()
        contains = sum(1 for low, high, _, _ in forced if any(g in truth for g in range(low, high + 1)))
        by_kind = collections.defaultdict(lambda: dict(intervals=0, holding=0))
        for low, high, kind, _ in forced:
            if all(g in in_scope for g in range(low, high + 1)):
                by_kind[kind]['intervals'] += 1
                by_kind[kind]['holding'] += any(g in truth for g in range(low, high + 1))
        target = max(1, round(len(record['gaps']) / mean_functions))
        cuts = segment(log_odds, record['blocked'], forced, target)
        chosen = sorted(cuts)
        folds[held] = dict(
            gaps=len(record['gaps']), truth=len(truth), blocked_true=sum(1 for g in truth if record['blocked'][g]),
            blocked_inside=sum(1 for g in record['gaps'] if g not in truth and record['blocked'][g]),
            forced_intervals=len(forced), forced_holding_a_true_boundary=contains, forced_by_kind=dict(by_kind),
            exact=boundary_metrics(chosen, truth), within_1=boundary_metrics(chosen, truth, 1),
            within_2=boundary_metrics(chosen, truth, 2))
        for gap, kinds in cuts.items():
            hit = gap in truth
            if kinds:
                forced_hits[kinds[0][0], 'cuts'] += 1
                forced_hits[kinds[0][0], 'hits'] += hit
            else:
                bucket = bucket_of(log_odds[gap])
                buckets[bucket][0] += 1
                buckets[bucket][1] += hit
        pooled['cuts'] += len(chosen)
        pooled['truth'] += len(truth)
        pooled['hits'] += len(set(chosen) & truth)
    rows = [data[name]['rows'][g] for name in data for g in data[name]['gaps']]
    labels = [v for name in data for v in data[name]['labels']]
    model = train(rows, labels)
    precision = {}
    for bucket, (cuts, hits) in sorted(buckets.items()):
        precision[bucket] = dict(cuts=cuts, hits=hits, precision=round(hits / cuts, 3))
    forced_precision = {}
    for (kind, field), value in forced_hits.items():
        forced_precision.setdefault(kind, {})[field] = value
    for row in forced_precision.values():
        row['precision'] = round(row.get('hits', 0) / row['cuts'], 3)
    total_p = pooled['hits'] / pooled['cuts'] if pooled['cuts'] else 0.0
    total_r = pooled['hits'] / pooled['truth'] if pooled['truth'] else 0.0
    return dict(schema='x2-tu-map-model/1', generated_by='tools/tu/map_infer.py calibrate',
                mean_functions=mean_functions, model=model, folds=folds,
                pooled=dict(pooled, precision=round(total_p, 3), recall=round(total_r, 3),
                            f1=round(2 * total_p * total_r / (total_p + total_r), 3) if total_p + total_r else 0.0),
                precision_by_log_odds=precision, forced_precision=forced_precision,
                function_estimator=score_functions())


# --------------------------------------------------------------- map generation


def hex32(value):
    return f'0x{value:08x}'


def load_annotations(path):
    names, homologs = {}, {}
    for line in Path(path).read_text().splitlines():
        match = re.match(r'0x([0-9a-fA-F]+),\s*([A-Za-z_][\w]*)', line)
        if not match:
            continue
        va = int(match.group(1), 16)
        names[va] = match.group(2)
        homolog = re.search(r'X1:\s*(\S+) in (\S+\.c)', line)
        if homolog:
            homologs[va] = (homolog.group(1), homolog.group(2))
    return names, homologs


def homolog_constraints(unit, homologs, in_scope):
    """X1 file of neighbouring homologs: same file holds the span, different files hold a cut.

    The rule cannot be scored on X1 itself, so it is limited to close neighbours and the
    file-path evidence overrides it. generate() reports how often the two disagree.
    """
    ordered = sorted(unit.index[va] for va in homologs if va in unit.index and unit.index[va] in in_scope)
    keep, cut = [], []
    for a, b in zip(ordered, ordered[1:]):
        if b - a > HOMOLOG_SPAN:
            continue
        first, second = homologs[unit.vas[a]][1], homologs[unit.vas[b]][1]
        if first == second:
            keep.append((a, b))
        else:
            cut.append((a, b - 1, 'x1_homolog', f'{first} -> {second}'))
    return keep, cut


def load_o0_homologs(root):
    """va -> (X1 function, X1 file, offset in X1's object, bytes) of -O0 bodies equal to X1's C at -O0."""
    path = root / O0_HOMOLOGS_FILE
    if not path.is_file():
        return {}
    return {int(row['va'], 16): (row['function'], row['file'].rsplit('/', 1)[-1], row['x1_offset'], row['size'])
            for row in json.loads(path.read_text())['functions']}


def o0_homolog_constraints(unit, homologs, in_scope):
    """As homolog_constraints, for whole compiled bodies: a span also needs X1's order of the two."""
    ordered = sorted(unit.index[va] for va in homologs if va in unit.index and unit.index[va] in in_scope)
    keep, cut = [], []
    for a, b in zip(ordered, ordered[1:]):
        (_, first, first_at, _), (_, second, second_at, _) = homologs[unit.vas[a]], homologs[unit.vas[b]]
        if first == second and first_at < second_at and b - a <= O0_HOMOLOG_SPAN:
            keep.append((a, b))
    large = [i for i in ordered if homologs[unit.vas[i]][3] >= O0_CUT_BYTES]
    for a, b in zip(large, large[1:]):
        first, second = homologs[unit.vas[a]][1], homologs[unit.vas[b]][1]
        if first != second and b - a <= O0_HOMOLOG_SPAN:
            cut.append((a, b - 1, 'x1_c_o0', f'{first} -> {second}'))
    return keep, cut


def function_table(unit, names, index_range):
    """Functions of one TU in the shape of the X1 map; `binding` is inferred, never read."""
    image = unit.image
    used = collections.Counter(names.values())
    out = []
    for i in index_range:
        function = unit.functions[i]
        va = function['va']
        name = names.get(va) or f'func_{va:08X}'
        if used[name] > 1:
            name = f'{name}_{va:08X}'
        out.append(dict(name=name, va=hex32(va), size=function['end'] - va,
                        file_offset=va + image.base, binding='unknown',
                        name_basis='annotation' if va in names else 'address'))
    return out


def suggest_name(unit, indices, names, homologs, ordinal):
    files = collections.Counter()
    for i in indices:
        for target in {ref[1] for ref in unit.refs[i]}:
            value = unit.strings.get(target)
            if value and is_source_file(value):
                files[value.decode('latin1')] += 1
    if files:
        stem = files.most_common(1)[0][0].rsplit('/', 1)[-1].replace('.euc.c', '.c')
        return dict(name=stem, confidence='high', basis='assert __FILE__ string used by this TU'), sorted(files)
    x1_files = collections.Counter(homologs[unit.vas[i]][1] for i in indices if unit.vas[i] in homologs)
    if x1_files:
        file_name, count = x1_files.most_common(1)[0]
        stem = file_name.rsplit('/', 1)[-1]
        return dict(name=stem, confidence='medium', basis=f'X1 file of {count} homolog function(s)'), []
    named = [names[unit.vas[i]] for i in indices if unit.vas[i] in names]
    if named:
        prefix = re.match(r'[a-z]+|[A-Z][a-z]+|[A-Z]+', named[0])
        stem = (prefix.group(0) if prefix else named[0]).lower()
        return dict(name=f'{stem}.c', confidence='low', basis=f'first named function {named[0]}'), []
    return dict(name=f'tu{ordinal:03d}.c', confidence='none', basis='no name evidence'), []


def boundary_record(cuts, gap, log_odds, model_file):
    """How one inferred cut was chosen, with the precision measured for that kind on X1."""
    if gap is None:
        return ['start of the scope zone'], 'zone'
    kinds = cuts[gap]
    if kinds:
        evidence = [f'{kind}: {note}' for kind, note in kinds]
        measured = model_file['forced_precision'].get(kinds[0][0])
        confidence = 'high' if kinds[0][0] in ('file_path', 'opt_level') else 'medium'
        if kinds[0][0] in ('dup_string', 'lit4_step', 'rodata_pad') and not (measured and measured['precision'] >= 0.5):
            confidence = 'low'         # the interval holds an edge, the chosen gap inside it may not be it
        if measured:
            evidence.append(f'exact-gap precision of this kind on X1: {measured["precision"]}')
        return evidence, confidence
    bucket = bucket_of(log_odds[gap])
    measured = model_file['precision_by_log_odds'].get(bucket)
    evidence = [f'model log-odds {log_odds[gap]:.2f}']
    if measured:
        evidence.append(f'precision of this score band on X1, held out: {measured["precision"]} ({measured["hits"]}/{measured["cuts"]})')
    confidence = 'medium' if measured and measured['precision'] >= 0.5 else 'low'
    return evidence, confidence


# First byte of an out-of-scope zone's piece in a data section, where the single-user rule cannot find it: a
# library references its own data at inner addresses, and a bare `lui` in library code can look like a
# reference to a round address inside game data. (zone stem, section, first byte, evidence), zone audit 2026-11-19.
ZONE_DATA_STARTS = [
    ('sce_sdk_low', '.data', 0x00646F20,
     'the libkernel banner "PsIIlibkernl2710" opens the Sony library .data right after elvKeepOffFlags[32] of '
     'tu273 (0x646EA0-0x646F20); everything from here to the CRI .data at 0x648688 is read by library code only'),
    ('cri_sofdec', '.rodata', 0x00690360,
     'the libpad texts run to "COMPLETE" at 0x690348 (scePadPortOpen reads 0x6902C8); the first CRI read is '
     '0x690360 (func_0014EAC0), before the E206011 texts'),
]


def attribute_data(unit, tus):
    """Per TU and section: span of the data only that TU references, and neighbours in address order."""
    image = unit.image
    owner = {}
    by_ordinal = {tu['ordinal']: tu for tu in tus}
    for tu in tus:
        for i in tu['_indices']:
            owner[i] = tu['ordinal']
    claims = collections.defaultdict(lambda: collections.defaultdict(list))
    users = collections.defaultdict(set)
    for table in (unit.users, unit.indexed_users):
        for target, who in table.items():
            users[target] |= who
    # At -O0 the compiler emits no .extern, so a $gp-relative access means the datum is defined in the
    # same object (user 2026-11-09, VSyncCount 0x69E084: $gp in xgl_render, lui in tu041). Such a datum
    # belongs to the one -O0 TU that reaches it through $gp, whoever else reads it through lui.
    gp_owners = collections.defaultdict(set)
    for i, refs in enumerate(unit.refs):
        if i in owner and image.in_o0_zone(unit.vas[i]):
            for _, target, kind, _ in refs:
                if kind == 'gp':
                    gp_owners[target].add(owner[i])
    defined = {target: owner[unit.index[function]] for target, function, _ in DATA_OWNERS
               if function in unit.index and unit.index[function] in owner}
    # One object that several TUs reach through different inner addresses: every referenced address inside
    # it, and its first byte, belong to the TU that defines it.
    for low, high, function, _ in DATA_OBJECT_OWNERS:
        if function in unit.index and unit.index[function] in owner:
            ordinal = owner[unit.index[function]]
            defined[low] = ordinal
            for target in users:
                if low <= target < high:
                    defined[target] = ordinal
    # Spans whose C definitions live in one TU although another TU is their only reader (see DATA_RANGE_OWNERS).
    for low, high, function, _ in DATA_RANGE_OWNERS:
        if function in unit.index and unit.index[function] in owner:
            ordinal = owner[unit.index[function]]
            defined[low] = ordinal
            for target in users:
                if low <= target < high:
                    defined[target] = ordinal
    for target, who in users.items():
        ordinals = {owner[i] for i in who if i in owner}
        if target in defined:
            continue
        if len(ordinals) == 1 and all(i in owner for i in who):
            section = image.section_of(target)
            claims[section][next(iter(ordinals))].append(target)
        elif len(gp_owners.get(target, ())) == 1:
            section = image.section_of(target)
            claims[section][next(iter(gp_owners[target]))].append(target)
    for target, ordinal in defined.items():
        claims[image.section_of(target)][ordinal].append(target)
    zone_ordinal = {tu['_zone']['stem']: tu['ordinal'] for tu in tus
                    if tu.get('_zone') and tu['_zone']['cls'] != 'game' and 'stem' in tu['_zone']}
    for stem, section, start, _ in ZONE_DATA_STARTS:
        if stem not in zone_ordinal:
            continue
        ordinal = zone_ordinal[stem]
        claims[section][ordinal] = [start] + [t for t in claims[section].get(ordinal, []) if t > start]
    section_order = {}
    rank = {tu['ordinal']: position for position, tu in enumerate(tus)}   # address order (SPLIT_CUTS ordinals)
    for section, per_tu in claims.items():
        order = sorted(per_tu, key=lambda ordinal: min(per_tu[ordinal]))
        block = 0
        blocks = [[order[0], order[0]]] if order else []
        for position, ordinal in enumerate(order):
            if position and rank[ordinal] < rank[order[position - 1]]:
                block += 1
                blocks.append([ordinal, ordinal])
            blocks[-1][1] = ordinal
            low, high = min(per_tu[ordinal]), max(per_tu[ordinal])
            previous = order[position - 1] if position else None
            following = order[position + 1] if position + 1 < len(order) else None
            sec = image.sections[section]
            lower = max(per_tu[previous]) + 1 if previous is not None else sec['va']
            upper = min(per_tu[following]) if following is not None else sec['va'] + sec['size']
            offset = image.offset_of(low) if sec['type'] != 8 else None
            by_ordinal[ordinal]['data'][section] = dict(
                anchored_start=hex32(low), anchored_end=hex32(high + 1), lower_bound_start=hex32(lower),
                upper_bound_end=hex32(upper), anchors=dict(code_ref_single_tu=len(per_tu[ordinal])),
                file_offset=offset, prev_object_in_section=previous, next_object_in_section=following,
                section_order_block=block)
        section_order[section] = dict(object_order=order, increasing_blocks=[
            dict(first_ordinal=first, last_ordinal=last) for first, last in blocks], common_tail=None)
    return section_order


# Data a TU defines although other TUs read it too: (datum, a function of the defining TU, why). The
# single-user rule of attribute_data cannot see these; compiler evidence can.
DATA_OWNERS = [
    (0x0069DB60, 0x002656E8,
     'sefEffectRecordCount (.sdata, initial value 64) is defined in the file of sefRestoreEffectRecords: the '
     'function is exact only when its file holds `int sefEffectRecordCount = 64;` (the cc1 gives a small-data '
     'declaration with a non-zero initializer its type alias set, so the count reload stays independent of '
     'the pointer and short field stores; with an extern declaration 34/60 words, with the definition 60/60 '
     'and the other 25 functions of the file unchanged), user 2026-11-15 "si es necesario recuperar los '
     'datos en C, hazlo". Since the 2026-11-15 merge that file is sef.c (old tu175 joined)'),
    (0x0069DAD0, 0x00251E30,
     'D_0069DAD0 (.sdata, int, zero) is read only by func_0025F958 of sef.c, but it sits before the "RESET" '
     'text of sef_print.c (0x69DAD8) and before the whole .sdata of sv_get_image_item.c (0x69DAE4-0x69DB40), '
     'whose objects both precede sef.o in link order; .sdata follows link order, so sef.c cannot define it. '
     'It is given to sef_print.c, the object whose data it adjoins; sef.c reads it as an extern. Without '
     'this the span of sef.c starts at 0x69DAD0 and its real run 0x69DB48-0x69DBE0 (sefEffectRecordCount, '
     'D_0069DB8C, ...) falls into the piece of sv_get_image_item.c (2026-11-15)'),
    (0x0068C5E8, 0x0026C0A8,
     'sefAlphaBlendValues: the GS ALPHA table { 0x44, 0x48, 0x8000000042, 0x6A } of sefBuildModelPacket (tu185), '
     'which reads entry 0 through a lui in a branch delay slot (the reference scan misses it) and entry 1 as '
     'D_0068C5F0; without this the .rodata of tu185 starts at entry 1 and sdv_181 holds entry 0 (data pass 7, '
     '2026-11-19)'),
]



# Data spans kept in the TU whose C defines them, although the single-user rule hands their labels to another
# TU or no code reads their first byte: (first byte, end, a function of the defining TU, why). The
# check is link order: within a link group the .data of the objects follows their .text order, so a reader
# later in that order cannot own data placed before an earlier object's data (reviewed 2026-11-17).
DATA_RANGE_OWNERS = [
    # Cross-section review (2026-11-17): every data section of one object follows the same link order, so a
    # span whose place contradicts the other sections of its TU belongs to the TU of that place (the readers
    # below are globals read through an extern; a jump table or literal moves its function instead).
    (0x002BD570, 0x002BD600, 0x001EEBC8, 'tya_draw_gauge.c: tyaGaugePacket (X1 tya_draw_gauge.c keeps this GS packet '
     'as its static TestEnv); the globals after it belong to ytachi_test, the next file'),
    (0x002BD600, 0x003E6DC0, 0x001EECD8, 'ytachi_test.c: the globals uwamonoStates, enepcLookAt*, uwamonoPoints, ladderSlots, '
     'uwamonoRecords, mapSceneScriptBuffer lie before tu067 in .data, while tu083 and init_uwamono_sys come after tu082 '
     'in .rodata, .sdata and .lit4; its .sdata and .rodata (scene names) interleave the same names, so one file holds them'),
    (0x003E7270, 0x003E7400, 0x001FD348, 'tu083.c: D_003E7270, read by tu083 and tu068; tu083 is the reader inside '
     'the link-order interval tu082..tu088'),
    (0x005E6D28, 0x005E6D40, 0x00266B50, 'sdv_180: its .data starts where srsRecordTable of srs.c ends (0x5E6D28, '
     'upstream b84ca21e); D_005E6D00, which tu185 reads, is the tail of that table (+0x630), not an object of sdv_180'),
    (0x00631140, 0x00631440, 0x0023E978, 'test_129.c: tiWaterTwoPi opens its .data (alignment 8 rules out a start '
     'at 0x631144); TiWater comes after test_129 in .rodata and .bss'),
    (0x0069DCA0, 0x0069DCAC, 0x0027C7D0, 'tu214.c: sellLabelTextOff lies between tu198 and tu214 in .sdata; tu234 '
     '(its reader) comes after tu229 in .data, .rodata and .lit4'),
    (0x0068C660, 0x0068C6F8, 0x00271458, 'sm_initilize: MMathUnitVector and MMathUnitMatrix, global constants read by '
     'eight TUs, lie between tu185 and tu187 in .rodata; m_math_191 (one reader) follows m_math_188 in .lit4'),
    (0x0068D2B0, 0x0068D348, 0x0027BFD0, 'tu214.c: jump table of effectRequestToCommand (see MOVED_CUTS 0x27C7D0)'),
    (0x006459A0, 0x00645F10, 0x001B8B18, 'tu042.c: the pxy glyph tables'),
    (0x00645F10, 0x00645FB0, 0x001B95F0, 'nml_model_set.c: after tu042 in .data as in .sdata and .text'),
    (0x0069D750, 0x0069D768, 0x0021A520, 'tu101.c: the record "\\031\\003", the 8-byte style record D_0069D758 and '
     'msgStyleReset (msgRangeDraw)'),
    (0x0069D768, 0x0069D770, 0x00228520, 'tu108.c: D_0069D768 (tu110 ptEnterSplit reads it at index-11), '
     'menuLoadPending and the selection counts'),
    (0x0069D7D0, 0x0069D888, 0x0022E9C0, 'yg2path.c: its globals, renderOriginX/Y (read by tu115) and its names'),
    (0x006ABBC0, 0x006ABC18, 0x0022A560, 'tu110.c: selectionFlagTable after its .bss (tu108 < tu110 in .data and .bss, '
     'so tu109 cannot follow tu110)'),
    (0x002B78C0, 0x002B7A90, 0x001D7250, 'tu049.c: mapObjectResourceNames is read only by tu051, but the .data of tu050 '
     '(0x2B7A90) lies between, and .data follows link order'),
    (0x002B7CF0, 0x002B9CF8, 0x001D9280, 'tu051.c, the camera code: cameraCfSlots (read by tu051 and tu077) starts its '
     '.data; tu050, before it, reads none of it'),
    (0x002B9CF8, 0x002BC1A0, 0x001DF120, 'res_get_path.c (old tu053 joined): the CD file name tables of xglCdFileGroups, '
     'then its .data around footMaterialKeys'),
    (0x006857A0, 0x00688238, 0x001DF120, 'res_get_path.c: the CD file names longer than 8 bytes (.rodata, 16-byte slots) '
     'of its name tables; tu052 (-G0) cannot hold them, as its short literals would be in .rodata too'),
    (0x0069B9B8, 0x0069CEE0, 0x001DF120, 'res_get_path.c: the CD file names of up to 8 bytes (.sdata literals of a -G8 '
     'file) of its name tables, emitted in reverse order per table'),
    (0x0069D1F0, 0x0069D218, 0x001EF600, 'tu067.c: the short native names of objxNatives (.sdata literals of the table)'),
    (0x00689960, 0x006899D0, 0x001F8138, 'runtime_uwamono_door.c (uwamonoRegisterNatives): the native names of uwamonoNativeTable (.rodata literals of the table)'),
    (0x00689A80, 0x00689CD0, 0x001F9CE8, 'runtime_camera.c: the native names of cameraNatives (.rodata literals; '
     'the short ones are already in its .sdata)'),
    (0x0069D398, 0x0069D3C0, 0x001FC558, 'runtime_math.c (mathRegisterNatives): the short names of mathNativeTable (.sdata literals, reverse order), '
     'then its "math"'),
    (0x00689CD0, 0x00689D20, 0x001FCBD0, 'runtime_menu.c (menuRegisterNatives): the long names of menuNativeTable '
     '(.rodata literals, reverse order)'),
    (0x00689D20, 0x00689D60, 0x001FCC00, 'uwamono_record_init.c: "PartsNames" (read by nothing; a resource name next to '
     'the object name rows uwamonoInitRecord scans), then the jump table of uwamonoInitRecord'),
    (0x0069D3C0, 0x0069D3F8, 0x001FCBD0, 'runtime_menu.c: the short names of menuNativeTable (.sdata literals) and its '
     'module name "menu"'),
    (0x0069D3F8, 0x0069D420, 0x001FCC00, 'uwamono_record_init.c: the short name literals uwamonoInitRecord compares'),
    (0x00689870, 0x00689940, 0x001F4F28, 'runtime_enepc.c: the long names of enepcNativeTable (.rodata literals, reverse '
     'order: "disableDrawFlags" .. "setParams"), right after the long names of runtimeNativeTable (0x689790-0x689870)'),
    (0x00689A60, 0x00689A80, 0x001F9CC0, 'get_ladder.c: the names of ladderNatives ("setEntrance", "setLadder", .rodata '
     'literals in reverse order)'),
    (0x0069D318, 0x0069D328, 0x001F9CC0, 'get_ladder.c: its module name "ladder" and uwamonoLadderState; the camera '
     'file reads neither'),
    (0x003E7020, 0x003E7040, 0x001F9CC0, 'get_ladder.c: ladderNatives (4 entries); the 16-aligned cameraRefAngOffset '
     'at 0x3E7040 opens the .data of runtime_camera.c'),
    (0x0069D328, 0x0069D398, 0x001F9CE8, 'runtime_camera.c: the short names of cameraNatives (.sdata literals, reverse '
     'order: "SetPers" .. "reset") and its module name "camera"'),
    (0x0069D770, 0x0069D7D0, 0x0022D170, 'talk_natives.c: mapActiveObject, talkResult, "NULL", the short names of '
     'talkNativeTable and segNativeTable (.sdata literals, reverse order) and the module names "talk", "g2path", "seg"'),
    (0x0069D1A0, 0x0069D1D0, 0x001EDCA0, 'elevator.c: the short names of elvNativeTable ("preload", "preren", "keepoff", '
     '"elvMove": .sdata literals, reverse order; .data 0x2BD480-0x2BD4A8 points at them and no code of yajima_test.c '
     'reads them) and its module name "id"'),
    (0x0068D270, 0x0068D2B0, 0x002733C0, 'fade_ctrl.c: the long names of fadeCommandTable ("Colorfade2", "Colorfade", '
     '"ColorSet": .rodata literals, reverse order; .data 0x63CEF8-0x63CF18 points at them, no code of cfeEvaluate.c '
     'reads them, whose last .rodata is the grid table read at 0x68D260) and the module name "Samplefade"'),
    (0x0069DC88, 0x0069DCA0, 0x002733C0, 'fade_ctrl.c: fadeSuppressFlags and the short names "Fadeout", "Fadein" of '
     'fadeCommandTable'),
    (0x0069DC7C, 0x0069DC88, 0x00272F38, 'm_se_obj.c piece, unchanged: the word 0x69DC7C (saveWriteImage and saveReadImage '
     'of pt_member.c read it through $gp) and "%s" at 0x69DC80 (read by tu150) lie between the .sdata of m_se_obj.c and '
     'fade_ctrl.c; pt_member.o comes much earlier in link order and cannot define them, and its only other .sdata '
     '(0x69D770) went to talk_natives.c, so without this entry the scan would hand the range to pt_member.c'),
    (0x0068A000, 0x0068A3F0, 0x00210268, 'tu100.c: the file names of its tables (.rodata literals; tu099 reads none)'),
    (0x003EB210, 0x003EBA58, 0x0021A520, 'tu101.c: also yg2PathFrameImage.. (yg2path) and D_003EB70C (tu109); those '
     'objects follow tu108 in link order, so their .data cannot lie before tu108\'s; tu100 reads D_003EB240, D_003EB268'),
    (0x003EBA58, 0x003EC458, 0x00228520, 'tu108.c: also items read only by tu100 (earlier in link order) and tu109'),
    (0x00645580, 0x006459A0, 0x001B2DE4, 'xgl_render.c (D_00645868 read only by tu100, a game object of another link group)'),
    (0x0063ED50, 0x0063EE80, 0x00199078, 'xgl_dma.c: tbl and the two 0x80-byte port records of PadData (X1 xgl_dma.c '
     'defines PadData[2] too), read through externs by the pad, camera and menu code'),
    (0x0063F680, 0x0063F6B0, 0x0019A890, 'xgl_cd.c: monthday[24], the month lengths and cumulative day offsets read by '
     'xglCd\'s date code (X1 xgl_cd.c defines monthday[24] too); the single-user rule had cut its first 20 bytes into the '
     'data of xgl_menu'),
]

# Objects that span addresses the single-user rule gives to several TUs: (first byte, end, a function of the
# defining TU, why). Xenosaga II has no symbol table, so a code reference to `object.field` or `table[i].field`
# is a label inside the object, and the single-user rule hands that label to whichever TU reads the field.
# Found while moving the data of the code-complete TUs to C (2026-11-15/16): a C object must start at its first
# byte and cannot be split between two objects.
DATA_OBJECT_OWNERS = [
    (0x005E66D0, 0x005E6D28, 0x00265820,
     'srsRecordTable: the 58-record SrsTable of srs.c (0x658 bytes, zero .data), one C object upstream (b84ca21e). '
     'sdv.c reads inside it, so the single-user rule had started the .data of sdv_180 at 0x5E6D00'),
    (0x0063F840, 0x006449A0, 0x0019DB58,
     'SaveData: the 0x5160-byte save image (zero .data), defined once in tu012.c, the memory card file, as in X1 '
     '(xgl_mc.c: `unsigned char SaveData[MC_SAVE_DATA_SIZE] = { 0 };`). It follows xglMcFileNames and ends where the '
     '.data of xgl_jpeg starts. Its readers use inner addresses (xgl_flags the flag window at +0x74, tu110 the '
     'blocks from +0x1274 on), so the single-user rule had cut it into tu012 (41 bytes), xgl_flags (0x63F878-'
     '0x640000) and, from the round address 0x640000 that an SDK `lui` seemed to reference, a piece of the Sony '
     'library zone that stayed scaffolding (zone audit 2026-11-19)'),
    (0x002B5D90, 0x002B78C0, 0x001D5440,
     'gameWork: the 0x1B28-byte game work block of tu048 (GameWorkPlayer, zero .data; 8 bytes of fill before tu049). '
     'Other modules read its fields through their own labels (gameLoopFlags = +8, elvSlopeTable, cameraBankAngle, '
     '...), so the single-user rule had cut its .data into pieces of tu075, tu100, res_get_path, tu051, tu077, '
     'tu085, tu089, tu052, tu069 and tu067, which have no .data of their own'),
    (0x006A18B0, 0x006A1B30, 0x001EDCA0,
     'elvRecords: 8 ElevatorRecords of 80 bytes (elevator.c, numbers 1..8). tu075 reads its rung table '
     'ladderStepHeights (0x6A1B30) at index-1 through the address 0x6A1B2C, which had started the .bss of tu075 '
     'inside the last record'),
    (0x0033FDC0, 0x003C6DC0, 0x001FFC90,
     'uwamonoRecords: 256 records of 0x870 bytes (0x33FDC0-0x3C6DC0, .data, zero). Every label in it is a field '
     'offset of record 0 used as an array base with stride 0x870 (0x3400F8 = +0x338 in tu110, uwamonoMoveRecords '
     '= +0x440, progEffectTable = +0x638 in init_uwamono_sys, +0x4..+0x334 in tu083). It lies between the data of '
     'tu083 and init_uwamono_sys in link order; given to init_uwamono_sys (include/main/tu051.h: "see '
     'init_uwamono_sys.c"), whose reset functions clear its columns'),
    (0x00644F18, 0x00644F48, 0x001A3FA8,
     'soundCtrl: the 0x30-byte SoundControl of include/main/tu022.h (song number +3, default volumes +6, the two '
     'slots +0x10 = soundSlots). The labels +0..+3 went to tu021, +4..+0xB to tu071 and +0xC.. to tu022; the '
     'struct starts 8-aligned at 0x644F18 and the slots array cannot start a section at 0x644F24'),
    (0x003EC608, 0x003EC668, 0x0022A560,
     'ptState: the 0x60-byte party state at 0x3EC608 (PartyState of tu110.c, the party module). Its head had '
     'gone to tu108 (saveDataSetDefaults reads it) and its last 12 bytes (ptSplitMode +0x54 .. +0x5C) to tu081, '
     'whose own first object is the 8-aligned selectionWindowList at 0x3EC668; the data before it '
     '(0x3EC458...) is already read only by tu110'),
    (0x006B4C30, 0x006B4C40, 0x002531C8,
     'svDrawList (sv_get_image_item.c): DrawList is 16 bytes at 0x6B4C30; func_002594E8 of tu162 writes its word '
     '+4 through its own label, so the single-user rule had started the .bss of tu162 at 0x6B4C34'),
    (0x0069D7E0, 0x0069D7E4, 0x0022DEF8,
     'the .sdata of tu115 starts at 0x69D7E0: its 8-byte arrays (0x69D7F0...) are 8-aligned in the object, so the '
     'object starts 8-aligned and the int at 0x69D7E0 (yg2PathImageFade, read only by yg2path) is its first datum'),
    (0x006A5A80, 0x006A5AF8, 0x00228520,
     'the 120-byte .bss object at 0x6A5A80: tu101 reads its first word, tu108 the rest through 0x6A5A84; one '
     'object, first of the .bss of tu108 (16-aligned start)'),
    (0x0068E158, 0x0068E178, 0x001A3248,
     '"sound\\vsf\\" and "sound\\smf\\" (.rodata 0x68E158, 0x68E168): the initializers of the .sdata pointers '
     'seBankDirectory / seMidiDirectory of tu021, emitted by that file before its other strings; no code names '
     'them, so they had stayed in the tail of the piece before (xgl_jpeg)'),
    (0x0069DF78, 0x0069DF80, 0x001A01D8,
     '"movie\\" (.sdata 0x69DF78, 7 bytes): the initializer of the pointer moviePathPrefix (0x69DF80) of tu017, '
     'emitted right before it; it had stayed in the tail of the item before (xglMcWildcardName of tu012)'),
    (0x002B7BB0, 0x002B7C88, 0x001D74E0,
     'mapObjectResources: 27 pairs of 8 bytes (0x2B7BB0-0x2B7C88, zero) that tu050 indexes; tya_draw_gauge reads '
     'words of it through its own labels (0x2B7BBC, 0x2B7BC0), so the single-user rule had cut it after 12 bytes'),
    (0x0068E1B8, 0x0068E1C8, 0x001A3FA8,
     'the string at 0x68E1B8 (.rodata): the initializer of the .sdata pointer adxPathPrefix (0x69DF90) of tu022, '
     'first string of that file; it had been the tail of the .rodata of tu021'),
]

# Inferred TU starts that are no object edge: (start, why). The TU joins the one before.
# Joined cuts whose merged TU keeps the LATER TU's ordinal (and with it its id and file name) instead of the first
# one's: merges published upstream by hand under the later name.
JOINED_KEEPS_LATER_ORDINAL = {
    0x0019F360,     # sce_ipu (tu015) into xgl_1_016 (tu016), upstream commit 9cc4558d
}

JOINED_CUTS = [
    (0x002661B0, 'srs.c: old tu178 joins (upstream 999a5bd1, review of #24): srs.c and tu178.c are one file, SrsTable is '
     'the one table layout and srsRecordTable one C object'),
    (0x002674A0, 'sdv_181.c: old tu182 joins (upstream 725299e3). tu181, tu182 and sdv_183 are one file in X1 (sdv.c), '
     'and sdvExecSpecial and sdvScheduleAlter match only when func_00267048 is compiled earlier in the same file '
     '(no EH region on the call, so reorg keeps beq)'),
    (0x00267BB8, 'sdv_181.c: old sdv_183 joins (upstream 725299e3, same evidence as 0x2674A0)'),
    (0x0026BC28, 'sdv_181.c: old tu185 joins (PR #28): the rodata cut fell inside sefAlphaBlendValues, which '
     'sdvDrawAlter indexes and sefBuildModelPacket reads; X1 sdv.c holds the same table as its static alphaTbl'),
    (0x00274070, 'oLib: tu202 joins (it lies between oLib and the old tu203, see 0x274100)'),
    (0x00274100, 'oLib: tu203 joins. Its .sdata flags xglCosDegTableReady/xglSinDegTableReady (read only by '
     'xglCosDeg/xglSinDeg) precede oLibHandle (read only by oLibInit/oLibExit) at 0x69DDC4-0x69DDD0, so the '
     'three file statics open one object (data map review 2026-11-17)'),
    (0x001DF120, 'main res_get_path, 2026-11-17 (data map review after gameWork): its .data (xglCdFileGroups, '
     'xglCdXapGroups before, resCommandTable.. after) surrounds footMaterialKeys, read only by the footMaterial '
     'functions of old tu053 (0x1DF090-0x1DF120): one object, old tu053 is the head of res_get_path.c'),
    # OV01 battle code, user 2026-11-17 ("unir tu025-tu032, por favor, hazlo"; the evidence starts at tu023):
    # four callers match only when their callee is an earlier C definition of the same file (reorg
    # nothrow annul rows, stub-definition probes exact): func_00A8F948 (old tu028) -> battleSlotGet
    # (0xA8AF10, old tu023); unitCanAct (old tu029) -> calcStatGet (0xA8E0C8, old tu026); actStockMaxGet
    # and actStockMake (old tu032) -> unitCanAct (old tu029); calcAttackDamage (old tu026) -> unitLevelAdd
    # (old tu025). So 0xA8A8B8-0xA92150 is one file.
    (0x00A8B930, 'ov01 battle file: battleSlotSet* (old tu024) inside the battleSlotGet (old tu023) -> '
                 'func_00A8F948 (old tu028) nothrow span 0xA8A8B8-0xA92150, user 2026-11-17'),
    (0x00A8BA68, 'ov01 battle file: unitLevelAdd (old tu025) must precede calcAttackDamage (old tu026), '
                 'nothrow annul row 0xA8CF34; user 2026-11-17'),
    (0x00A8BC60, 'ov01 battle file: old tu026 inside the span (see 0x00A8BA68)'),
    (0x00A8F100, 'ov01 battle file: old tu027 inside the span (see 0x00A8B930)'),
    (0x00A8F658, 'ov01 battle file: func_00A8F948 (old tu028) needs battleSlotGet (old tu023) as an earlier '
                 'definition (beql/beq annul row; stub probe 51/51); user 2026-11-17'),
    (0x00A90368, 'ov01 battle file: unitCanAct (old tu029) needs calcStatGet (old tu026) as an earlier '
                 'definition (bnel/bne at 0xA90604; stub probe 30/30); user 2026-11-17'),
    (0x00A90660, 'ov01 battle file: old tu030 inside the span (see 0x00A90368)'),
    (0x00A912D0, 'ov01 battle file: old tu031 inside the span (see 0x00A90368)'),
    (0x00A91910, 'ov01 battle file: actStockMaxGet/actStockMake (old tu032) need unitCanAct (old tu029) as '
                 'an earlier definition (annul rows 0xA91C9C, 0xA91E54); user 2026-11-17'),
    (0x00292C40,
     'tiMRPackageSetTableAndEntryA (0x292CA0) is exact only when tiMRPackageSetTableA (0x2920C8, TiMRPackage.c) is an '
     'already compiled definition of the same file (reorg nothrow: beqz, not beqzl); the merged TiMRPackage.c + old '
     'tu253 compiles all 31 functions byte-identical at their VAs. The cut had log-odds -2.03 (X1 band precision '
     '0.23) and splits the SelectEntryA/SelectEntryB twins of the A/B setters. User 2026-11-16'),
    (0x0019F360,
     'setD4_CHCR (0x19F300, the former one-function TU sce_ipu) is static in the movie file: upstream commit 9cc4558d '
     'merged TU sce_ipu into xgl_1_016 (make_gstrans is the first function of the old cut); pulled 2026-11-15'),
    (0x001EAB08,
     'SoundTest (0x1EA2C8) and MemcardTest (0x1EAF88) both print the static menu cursor ">" at '
     '0x69D028, a static char[8] (X1 yajima_test.c menu_cursor), and X1 defines both in yajima_test.c; '
     'found matching MemcardTest, user 2026-11-08'),
    (0x00268BF8,
     'sdvTransOffset (tu183) and sdvExecSpecial (sdv_184) load the same .rodata constant D_0068C590, and the '
     'jump table of sdvDrawSpecial (0x68C5A0) lies inside the .rodata run of the camera functions; X1 sdv.c '
     'defines the camera code, sdvTransOffset and the special/alter code in one file. Found matching '
     'sdvDrawSpecial, user 2026-11-08'),
    # -O0 zone (main 0x1a75a8-0x1d5440), user 2026-11-08/09: at -O0 a $gp access means a definition in the
    # same file, and the header static helpers (xglVectorCopy/Zero/AddXYZ/SubXYZ, xglMatrixCopy) are emitted
    # once per object at its END, called only by that object.
    (0x001A76BC,
     'xgl_2: iRandSeed (.sbss 0x69E398, static in X1 xgl_2.c) is read $gp-relative by xglSRand/xglLRand '
     '(0x1A75A8) and by xglRandSeedInit/func_001AB2F0 (0x1A7DC8 on), so 0x1A75A8-0x1AF494 is one file'),
    (0x001A7C9C, 'xgl_2: inside the iRandSeed span 0x1A75A8-0x1AF494 (see 0x1A76BC)'),
    (0x001A7DC8, 'xgl_2: inside the iRandSeed span 0x1A75A8-0x1AF494 (see 0x1A76BC)'),
    (0x001AF32C,
     'xgl_2: 0x1AF32C-0x1AF494 are the static header copies (xglVectorCopy, ...) that end the xgl_2 object; '
     'only xgl_2 functions call them'),
    (0x001CC998,
     'nml_model_set: the static copies 0x1CC998-0x1CCAA0 (xglVectorCopy, xglMatrixCopy, xglVectorClear, '
     'AddXYZ, SubXYZ) are called only by nml_model_set functions (pxySetupShadowProjector, pxySubmitModel, '
     '...), so they are its tail'),
    (0x001D3FB8,
     'tu045: func_001CDC40 calls the static copy xglVectorSubXYZ 0x1D4530 at the end of this span, and '
     'func_001D4498 reads .sdata 0x69E10C $gp-relative in the middle of tu045\'s .sdata run'),
    (0x001E0B10,
     'res_get_path joins tu054: func_001E4000 (0x1E4000) calls func_001DF9F0 (tu054) and matches only when '
     'that callee is an earlier C definition in the same file (nothrow same-TU call, bnezl annul at '
     '0x1E40A8); found matching func_001E4000, user 2026-11-09'),
    (0x00232018,
     'ysegf joins tu118: ysegfRunMenu (0x2334C0) calls ysegfRunState (0x231F78) and matches only when that '
     'callee is an earlier C definition in the same file (nothrow same-TU call: reorg then sees a0 set '
     'before use after the call and fills the triangle beqz at 0x23355C with lui a0); found matching '
     'ysegfRunMenu, 2026-11-13'),
    (0x002320B0, 'ysegf: inside the ysegfRunState..ysegfRunMenu span 0x231F78-0x2334C0 (see 0x232018)'),
    # sv_get_image_item (old tu157-tu161), 2026-11-13, user order "la fusion antes de integrar": every cut
    # here was a low-confidence model cut (X1 precision 0.19-0.27). X1 sv_get_image_item.c holds
    # svLoadImageList, svAdd*, svAnalyzeChunk, svInitImageMapper, svDeleteImageMapper* and SGs* in this
    # order; the externally called tu157 draw-list helpers (svSortAdd, svDrawList*) are called only from
    # tu161, svImageListAlloc/Destroy (tu158) only from tu159, and tu160 calls svDeleteImageMapper.
    (0x00253B18, 'sv_get_image_item: old tu158 joins tu157 (see the block comment)'),
    (0x00254280, 'sv_get_image_item: old tu159 joins (see 0x253B18)'),
    (0x00254AA0, 'sv_get_image_item: old tu160 joins (see 0x253B18)'),
    (0x002551D0, 'sv_get_image_item: old tu161 joins (see 0x253B18)'),
    # sef (old tu164-tu176), same order: low-confidence model cuts. sefMemZero (old tu169, X1 sef.c:1179)
    # and sefMemFillWords are called only from tu164; eight tu170 functions (sefGetKindMatrix,
    # sefResetObjects, ...) whose X1 counterparts (sefGetPoint, static sefGetCirclePos/SpherePos) live in
    # sef.c, and the tu171-tu173 helpers, are called only from tu164, so the contiguous span is one file.
    (0x0025FB90, 'sef: old tu165 joins tu164 (see the block comment)'),
    (0x00260840, 'sef: old tu166 joins (see 0x25FB90)'),
    (0x00260D38, 'sef: old tu167 joins (see 0x25FB90)'),
    (0x002612B0, 'sef: old tu168 joins (see 0x25FB90)'),
    (0x00261890, 'sef: old tu169 joins (see 0x25FB90)'),
    (0x00261960, 'sef: old tu170 joins (see 0x25FB90)'),
    (0x00263BF8, 'sef: old tu171 joins (see 0x25FB90)'),
    # 0x263E68 and 0x2647E0 join again (2026-11-15, after cc1 fsv14). They had been restored as cuts on
    # 2026-11-13/14 because sefSpawnEffect (0x263E88) and sefApplyRecordMode (0x264C78) match only when their
    # callees sefStartEffectKeyDefault (0x260EE8) and sefReleaseEffectRecord (0x25DDE0) are not yet-compiled
    # definitions at the call (reorg: bnez/bnel instead of the same-file form). A block-scope `extern`
    # prototype in the caller gives the same code inside one file (verified: all 228 functions of the span
    # byte-identical), and two facts require one file: (1) the .sdata run 0x69DB48-0x69DBE0 interleaves data
    # used only by the old tu175 code (0x69DB6C, sefCurrentOwner 0x69DBC4) and only by the old tu172 code
    # (sefFlagWord 0x69DBC0, 0x69DBD0, 0x69DBD8) with data used only by the sef code (0x69DB64-0x69DB74,
    # sefRandSeed 0x69DBC8, sefNoiseReady 0x69DBCC), which separate objects cannot produce; (2) both
    # sefRestoreEffectRecords (0x2656E8, old tu175) and func_0025EBE0 / func_0025CAB8 (sef) need the typed
    # small-data definitions `int sefEffectRecordCount = 64;` / `float D_0069DB8C = 1.0f;` in their own file
    # (reports/cc1-fsv14-typedkill.md).
    (0x00263E68, 'sef: old tu172 joins (see the block comment above)'),
    (0x00264418, 'sef: old tu173 joins (see 0x263E68)'),
    (0x00264670, 'sef: old tu174 joins (see 0x263E68)'),
    (0x002647E0, 'sef: old tu175 joins (see 0x263E68)'),
    (0x00265300, 'sef: old tu176 joins (see 0x263E68)'),
    (0x00AA5B60, 'ov01 tu057 joins tu056 (2026-11-18): menuTgtStatusNew takes the address of menuTgtStatusDraw, and '
     'func_00AA60B8 (tail of old tu057) is called only by menuWepSpPanelDraw (tu056): one file'),
    (0x00AA8678, 'ov01 tu065 joins tu064 (2026-11-18): screenFadeInStart/OutStart take the addresses of '
     'screenFadeInStep/OutStep/Draw, referenced nowhere else: static callbacks of one file'),
    (0x00AB2940, 'ov01 tu087 joins tu086 (2026-11-18, after the move to MEfCreateSpray): StepSprayPoints, DrawSpray and '
     'TickLifetime121 are callbacks whose address only MEfCreateSpray takes'),
    (0x00AB40D8, 'ov01 tu090 joins tu089 (2026-11-18, after the move to func_00AB3CC8): func_00AB40D8 and '
     'MEfAdvanceHistoryStep are callbacks whose address only func_00AB3CC8 takes'),
    # X1 file audit 2026-11-19 (.work/x2-tu-audit, user order: TU limits by X1's files)
    (0x0019F300, 'xgl_1: the old tu015/tu016 pair joins tu014 (myDmaDirectFromIPU, X1 xgl_1.c#0, then make_gstrans #2): one X1 file, same -G0 contract, no data between'),
    (0x001B74D8, 'nml_packet_add: _CurMatrixSet (X1 nml_packet_add.c#0) and _CurApplyMatrix.._CurMatrixMul (#2) are one X1 file; -O0, no data'),
    (0x00271DA8, "m_math: tu187 (MMathCalcRotFar, X1 m_math.c#3) and tu188 (MMathCalcHermite #4); tu187's .bss holds MOutputDebugBuffer above MMathRandomState, so the debug-string head is in the same object"),
    (0x00271EF8, 'm_math: tu189 continues m_math.c (MMathRad2DegVectorI #9 -> MMathNormalizeVector2 #10); tu189-tu195 read no small data, so their -G0 contract was undetermined'),
    (0x00271FE8, 'm_math: tu190 (MMathCalcLengthYZ #14 -> MMathCalcDist #15)'),
    (0x00272088, 'm_math: tu191 (MMathCalcDirVector #17 -> MMathCalcDir #18)'),
    (0x00272370, 'm_math: tu192 (MMathIsVectorEqual4 #31 -> MMathVectorDotProduct #32)'),
    (0x002723F0, 'm_math: tu193 (MMathVectorInterpolation #34 -> MMathRotateMatrixX #36)'),
    (0x00272928, 'm_math: tu194 (MMathRotateMatrixYXZ #42 -> MMathApplyMatrix #43)'),
    (0x00272A20, 'm_math: tu195 (MMathScaleMatrix #45 -> MMathScaleMatrixS #46)'),
    # Script native modules (2026-11-20, see MOVED_CUTS 0x1F2DF8): the functions of one native table are one file.
    (0x001F34A8, 'runtime.c: old tu069 joins. runtimeNativeTable lists functions on both sides (runtimeItem .. '
     'runtimeGetAnalogR of old tu068, runtimeProgEffect .. runtimeSystem of old tu069), its short names are the '
     '.sdata literals 0x69D220-0x69D2C8 that sat in old tu068 and its long names the .rodata literals 0x689790.. '
     'that sat in old tu069'),
    (0x001F48F8, 'runtime.c: old tu070 joins (mpeg2, adx, adxLoad, adxPlay are entries of runtimeNativeTable; they '
     'touch no small data, which is why the flag inference had read -G0)'),
    (0x001F4A20, 'runtime.c: old tu071 joins (runtimeSe .. runtimeBgm are entries of runtimeNativeTable)'),
    (0x001F5E08, 'runtime_enepc.c: old tu073 joins. enepcNativeTable (.data 0x3E6F58) lists functions on both sides '
     '(enepcSetParams, enepcSetParamsAll, enepcMotion of old tu072; motionFace .. enepcEnableDrawFlags of old tu073), '
     'and its short names are the .sdata literals 0x69D2D0-0x69D300 of old tu073'),
    (0x001F9928, 'get_ladder.c: old tu076 joins. ladderInitTable .. ladderSetEntrance lie between the X1 get_ladder.c '
     'functions (Fly_to_Ladder .. Ladder_Main) and ladderRegisterNatives; ladderSetLadder and ladderSetEntrance are the '
     'entries of ladderNatives (.data 0x3E7020), and uwamonoLadderState (.sdata 0x69D320, after the module name '
     '"ladder") is written by ladderInitTable and the Ladder functions and read by no camera function'),
    (0x001FC620, 'runtime_menu.c: old tu080 joins. menuNativeTable (.data 0x3E7210) lists ptInit, ptClear (old tu079), '
     'ptJoin .. ptSet (old tu080) and ptSetPilot .. ptTent (old tu081); its names are the literals 0x689CD0-0x689D20 '
     'and 0x69D3C0-0x69D3F0, followed by the module name "menu" of menuRegisterNatives'),
    (0x001FC888, 'runtime_menu.c: old tu081 joins (ptSetPilot, ptGetLeader, umnReturn, ptSetArray, ptTent are entries of '
     'menuNativeTable; they touch no small data, which is why the flag inference had read -G0)'),
    (0x0022D4F0, 'talk_natives.c: old tu112 joins. talkNativeTable (.data 0x3EC868) lists talkCheck, talkReset (tail of old '
     'tu110), talkSmsg, talkMsgDtl (old tu111), talkWinClose .. talkWait (old tu112), menuAddItem .. slotWinOpen (old '
     'tu113) and slotWinGetResult (old tu114); its names are the .sdata literals 0x69D780-0x69D7B0 and the .rodata '
     'literals 0x68AD10-0x68ADC0. Old tu112 touches no small data, which is why the flag inference had read -G0'),
    (0x0022D5A0, 'talk_natives.c: old tu113 joins (segAddrOnMessage, segAddrOnGetFlag = segNativeTable 0x3EC918; setQuestFlag .. '
     'getCitizenFlag = g2pathNativeTable 0x3EC8F0; menuAddItem .. slotWinOpen = talkNativeTable). The name "addrOn" of '
     'segNativeTable is the .sdata literal 0x69D7B0 next to the talk names, so the file is -G8; old tu113 compiles '
     'byte-identical under -G8 (it touches no small data)'),
    (0x0022DD20, 'talk_natives.c: old tu114 joins (slotWinGetResult, scriptRegisterNatives and the three tables). '
     'mapObjectSetActive .. talkSetResult (0x22DDF8-0x22DEF8) read mapActiveObject and talkResult (0x69D770, 0x69D774), '
     'the first .sdata of this object, so the file runs past scriptRegisterNatives to talkSetResult'),
    (0x001EDA58, 'yajima_test.c: old tu063 joins. yajimaTestRows (.data 0x2BD3E8) points at the labels "EDGE" .. "FONT" '
     '(.sdata 0x69D150-0x69D198, reverse order, the .sdata literals of the table) and "RESOURCE" (.rodata 0x689480), '
     'which sit before "SELECT" (0x69D198) and the clock format (0x689490) YajimaTest reads: old tu063 has .sdata, so '
     'it is -G8 and not -G0 (it compiles byte-identical under -G8), and in X1 YajimaTest is the last function of '
     'yajima_test.c, after the static tests its table lists'),
    (0x00273A20, 'fade_ctrl.c: old tu199 (fade_commands.c) joins. fadeCommandTable (.data 0x63CEF8, right after fadeState '
     '0x63CE98) has its short names "Fadeout"/"Fadein" as .sdata literals at 0x69DC90, right after fadeSuppressFlags '
     '(0x69DC88) of the fade functions, and its long names at .rodata 0x68D270 before the module name "Samplefade"; '
     'a file with .sdata literals is -G8, and old tu199 touches no small data (compiles byte-identical under -G8), '
     'which is why the flag inference had read -G0. The file ends with fadeRegisterCommands'),
    (0x00272D38, 'm_math: tu196 (MMathRotTransPers #51 -> MMathRotTransPersClip #52); the joined .bss/.lit4/.rodata/.sdata pieces are contiguous'),
    (0x00296688, 'xrg_rand_int: XrgScaleVector (X1 xrg_rand_int.c#15) and XrgInnerVector (#23) are one X1 file, same contract, no data'),
    (0x00A84430, 'ov01 unit_cmd: tu011 joins tu010 (unitEnCreate, X1 unit_cmd.c#10 -> unitRemove #14); tu010-tu016 data pieces are contiguous'),
    (0x00A84830, 'ov01 unit_cmd: tu012 joins (unit_cmd.c homologs on both sides)'),
    (0x00A85A80, 'ov01 unit_cmd: tu013 joins (unitCmdClear #24 -> unitCmdDmg #45)'),
    (0x00A862D8, 'ov01 unit_cmd: tu014 joins (unitCmdMot #50 -> unitCmdMoveChaRun #58)'),
    (0x00A865D8, 'ov01 unit_cmd: tu015 joins (unitCmdMovePos #66 -> unitCmdBted #91)'),
    (0x00A86CB0, 'ov01 unit_cmd: tu016 joins (X1 unit_cmd.c#95..); objPosePublish/objPoseFetch keep their documented '
     'one-site doubleword views of XenoActor.position and SlideBlock.position'),
    (0x00AA3928, 'ov01 menu: tu050 joins tu049 (X1 menu.c on both sides, X2 reordered); tu049-tu054 .bss/.data/.rodata contiguous'),
    (0x00AA3E28, 'ov01 menu: tu051 joins (menu.c on both sides)'),
    (0x00AA4280, 'ov01 menu: tu052 joins (menu.c on both sides)'),
    (0x00AA4670, 'ov01 menu: tu053 joins (between menu.c homologs menuKeyGuideDisp #62 and objDmgNumCreate #68)'),
    (0x00AA4BD8, 'ov01 menu: tu054 joins (objDmgNumCreate, menu.c #68)'),
    (0x00AAAA50, 'ov01 debug_entry: tu073 joins tu072 (X1 debug_entry.c on both sides), data contiguous'),
]

# Cuts moved on compiler evidence: (inferred start of a TU, its corrected start, why).
MOVED_CUTS = [
    (0x00274808, 0x00274830,
     'wrapIntRange (0x274808) ends tu206, the xglClampInt object (upstream 4cb7d47b): recordStepAngle of tu207 keeps '
     'bnel on its call without a block-scope declaration only when the callee is not compiled earlier in its file'),
    (0x0027C7D0, 0x0027BFD0,
     'effectRequestToCommand reads its jump table at 0x68D2B0, between tu199 and tu225 in .rodata where the '
     'tu214 file sits, while the gsQueue functions before it keep tu213 .sdata/.sbss/.lit4 after tu211; the '
     'effect functions open the tu214 file (data map review 2026-11-17)'),
    (0x001B73B0, 0x001B74A0,
     '-O0 static-copy tail: xglVectorZero/AddXYZ/SubXYZ 0x1B7408-0x1B74A0 are called only by face_point '
     '(_CheckLine, setup_occlusion), so they and the global func_001B73B0 before them end face_point, user '
     '2026-11-08'),
    (0x001CCA30, 0x001CCAA0,
     '-O0 static-copy tail: AddXYZ/SubXYZ 0x1CCA30-0x1CCAA0 are called only by nml_model_set, so tu045 starts '
     'at func_001CCAA0, user 2026-11-08'),
    (0x00249248, 0x0024B0E8,
     'cfeDrawRotationGrid and func_00249ED0 initialize vectors at 0x68D220-0x68D26F, right after the jump table '
     'of cfeCommandEvaluate (0x68D200) in one increasing .rodata run, while the next file starts its '
     '.rodata (0x68CD10, below) with cfeListSetKeyMode; the functions between use no .rodata, so they stay '
     'with cfeEvaluate. Found matching func_00249ED0; a string-free extension of rodata_order_breaks '
     'held on X1 (13/13) but gave false cuts on X2 at file-scope tables (0x68F0D0), user 2026-11-08'),
    # ov01 MEf effect files (2026-11-18, user "fusionar todas las TUs que puedas"): every effect Create is referenced
    # only from the .data effect table (global) and is the only function that takes the address of the callbacks and
    # helpers after it (static), as X1's m_ef_create_*.c (global MEfCreate_XXX, then static fnXXX_PR/DM/DP/PO). The
    # inferred cuts fell one Create late: each file starts at its Create.
    (0x00AADB58, 0x00AADBE0, 'ov01 MEf: MEfAdvanceStepShort is func_00AACDA0\'s callback; the Sway file opens at MEfCreateSway'),
    (0x00AAE0A8, 0x00AAE748, 'ov01 MEf: MEfDrawStencilA, func_00AAE148, func_00AAE478 are MEfCreateSway\'s; next file opens at func_00AAE748'),
    (0x00AAEE00, 0x00AAEEE8, 'ov01 MEf: func_00AAEE00 is func_00AAE748\'s callback (its object layout differs from func_00AAEEE8\'s)'),
    (0x00AAFA50, 0x00AB0088, 'ov01 MEf: func_00AAFA50..MEfTickLifetime28 are MEfCreatePathEffect25\'s; next file opens at MEfCreatePathEffect31'),
    (0x00AB1050, 0x00AB0A38, 'ov01 MEf: func_00AB1050, MEfDrawRotatedQuads, MEfTickTrailSample are MEfCreatePathEffect29\'s; its file opens at it'),
    (0x00AB1890, 0x00AB14E0, 'ov01 MEf: MEfSetStepFlags30, MEfDrawFlagWindow, MEfAdvanceFlagWindow are func_00AB14E0\'s'),
    (0x00AB2040, 0x00AB2750, 'ov01 MEf: MEfDrawWaveEffect, MEfAdvanceHistoryTrail are MEfCreateWaveEffect\'s; the spray file opens at MEfCreateSpray'),
    (0x00AB3668, 0x00AB3CC8, 'ov01 MEf: MEfDrawPathTrail, MEfAdvancePathStep are MEfCreateActorPath\'s; next file opens at func_00AB3CC8'),
    # Script native modules (2026-11-20, .work/x2-natives-bounds-20261120): a native table `SbNativeMethod xNatives[]`,
    # the string literals of its names and the functions it lists are one object, and each module file ends with its
    # `xRegisterNatives` (objxRegisterNatives ends uwamono_objx, cameraRegisterNatives runtime_camera). The inferred
    # cuts fell one or two functions late or early: each file starts right after the previous module's RegisterNatives.
    (0x001F2DF8, 0x001F18F0,
     'runtime.c opens at runtimeSetNumber (0x1F18F0), the first function listed in runtimeNativeTable (.data 0x3E6E40) '
     'and the one after objxRegisterNatives, which ends uwamono_objx.c; X1 runtime.c is the homolog file'),
    (0x001F4E20, 0x001F4F28,
     'runtimeVibration (last entry of runtimeNativeTable) and runtimeRegisterNatives (0x1F4F00) end runtime.c; the '
     'enepc file opens at enepcSetParams, the first function listed in enepcNativeTable'),
    (0x001F6D90, 0x001F6F10,
     'disableDrawFlags (last entry of enepcNativeTable) and enepcRegisterNatives (0x1F6EE8) end the enepc file; the '
     'uwamono natives open at uwamonoSetDoorAll, the first function listed in uwamonoNativeTable'),
    (0x001F8138, 0x001F8160,
     'uwamonoRegisterNatives (0x1F8138) ends the uwamono natives file (uwamonoSetDoorAll .. setCollisionScale are the '
     'entries of uwamonoNativeTable, .data 0x3E6FF0); the ladder file opens at uwamonoCompassSector, a helper called '
     'only by On_Ladder'),
    (0x001F9CC0, 0x001F9CE8,
     'ladderRegisterNatives (0x1F9CC0) ends the ladder file get_ladder.c; runtime_camera.c opens at cameraCentiToFloat '
     'and ends with cameraRegisterNatives'),
    (0x001FC558, 0x001FC580,
     'mathRegisterNatives (0x1FC558) ends runtime_math.c (mathAtan, mathSin, mathCos, mathRandom are the entries of '
     'mathNativeTable, .data 0x3E71E8); the menu natives open at ptInit, the first function listed in menuNativeTable'),
    (0x001FCBD0, 0x001FCBF8,
     'menuRegisterNatives (0x1FCBD0) ends the menu natives file runtime_menu.c; menuSceneSetupHook and '
     'uwamonoInitRecord stay a file of their own (uwamono_record_init.c): the 8 zero bytes at 0x689D58 between the jump '
     'tables of uwamonoInitRecord (0x689D30, 10 words) and uwamonoReleaseRecord (0x689D60) are the 16-byte .rodata '
     'alignment of a new object (a second jump table of one object would start 8-aligned at 0x689D58), so they do not '
     'join uwamono_record.c either'),
    (0x0022D1D8, 0x0022D170,
     'talkCheck (0x22D170) opens the talk natives file talk_natives.c: it is the first entry of talkNativeTable, and '
     'talkCheck/talkReset read talkResult (.sdata 0x69D774), which no function of pt_member.c (ptGetMenuMember .. '
     'ptGetStatusBlock) reads; mapActiveObject and talkResult (0x69D770-0x69D778) lie right before "NULL" and the '
     'native names of the same object'),
    (0x0020CE30, 0x0020CE40,
     'func_0020CE30 as C earlier in the file turns bnel into bne in func_0020DDC8; defined after the caller '
     'it matches, so the original did not see that definition first (docs/bulk-match.md, user 2026-11-05)'),
    # X1 file audit 2026-11-19 (.work/x2-tu-audit, user order: TU limits by X1's files)
    (0x001948D8, 0x001948B8, 'xglTimer0Reset is X1 xgl_timer0.c#0: it opens the xgl_timer0 file, not control_entry'),
    (0x00199428, 0x00199430, 'xglDmaInitial is X1 xgl_dma.c#10: it ends xgl_dma; xgl_pad opens at xglPadInitial'),
    (0x001A4EC8, 0x001A4EC0, 'xglHddDummyCB is X1 xgl_hdd.c#0: it opens xgl_hdd'),
    (0x002068B0, 0x00205A20, 'set_motion opens at Enemy_After_Battle (X1 set_motion.c#18); tu085 keeps the enemy update '
     'code (X1 enemy_1.c), both now on the shared UwamonoRecord of ytachi_test.h'),
    (0x00208560, 0x00208B28, 'uwamonoGetSectorEnds and Check_InsideID (X1 call_java_method.c#10) end uwamono_event_check; vibration opens at Vibration_Stop (X1 vibration.c#0)'),
    (0x00266F68, 0x00266D78, 'sdv opens at sdvInitAmbient (X1 sdv.c#0); tu180 keeps the sc_get functions (X1 sc_get.c); .sdata clean at the cut'),
    (0x00A814A8, 0x00A811F8, 'ov01 obj: objStdMove (X1 obj.c#16) opens the obj file, tu006 keeps gr_gp_init (#26)'),
    (0x00AA2CE0, 0x00AA2D18, 'ov01 GS_REG_SET is X1 gr_gp_init.c#29: it ends gr_gp_init (tu048); menu opens at menuPanelRowsReset'),
]


# New TU starts inside one inferred TU: (unit, start, ordinal, X1 file name, why). The new TU takes the given ordinal,
# outside the address order of the others, so no existing TU is renumbered (X1 file audit 2026-11-19 (.work/x2-tu-audit, user order: TU limits by X1's files)).
SPLIT_CUTS = [
    ('main', 0x00194890, 300, 'xgl_thread.c',
     "xglSleep is X1 xgl_thread.c (between control_entry's main and xgl_timer0); no data on either side"),
    ('main', 0x001AB3C0, 301, 'xgl_light.c',
     '-O0 static-copy tail xglMatrixCopy/xglVectorSubXYZ (0x1AB344-0x1AB3C0, called only by xgl_2 code) ends xgl_2; xglLightSetDefault.. is X1 xgl_light.c with its own copies at its end; data clean'),
    ('main', 0x001B2120, 302, 'xgl_packet.c',
     '-O0 static-copy tail 0x1B2008-0x1B2120 ends xgl_camera; xglPacketTextureTrans..xglPacketSend are X1 xgl_packet.c (6/6); data clean'),
    ('main', 0x001E9008, 303, 'game_disk_change.c',
     'GameDiskChange is X1 game_disk_change.c, Intermission X1 game_over.c; .data/.rodata/.bss clean at the cut'),
    ('main', 0x001EECD8, 304, 'ytachi_test.c',
     'YtachiTest is X1 ytachi_test.c, tyaDrawGauge X1 tya_draw_gauge.c; data clean at the cut'),
    ('main', 0x002025B0, 305, 'pause_menu_page_uwamono.c',
     'DispCollUwamono..DispPillar are X1 pause_menu_page_uwamono.c, HitCheckBoxUwamono X1 init_uwamono_sys.c; .lit4 clean at the cut'),
    ('ov01', 0x00A9BC08, 91, 'think_dispatch.c',
     'msgReset opens the message, party data and effect senders of upstream PR #30 (tu042 keeps the bat.dat getters); '
     'its .rodata 0x3B6A0 and .bss 0xAC51B0 go with it'),
    ('ov02', 0x00A80790, 300, 'boot_display.c',
     'BootDisplay is X1 boot_display.c, LogoFirst X1 logo.c; no data'),
    ('ov02', 0x00A81C38, 301, 'title_game_init.c',
     'TitleGameInit is X1 title_game_init.c, Title X1 title.c; no data'),
]

# File names from X1 for TUs that are one X1 file: (unit, first function, name).
X1_FILE_NAMES = [
    ('main', 0x001AF494, 'xgl_camera.c'),
    ('main', 0x00205A20, 'set_motion.c'),
    # X1 call_java_method.c; X2 has no Java (uwamonoSendEvent posts to the .sb VM), so a neutral name (user 2026-11-19)
    ('main', 0x00207028, 'uwamono_event_check.c'),
    ('main', 0x00208B28, 'vibration.c'),
    ('main', 0x00266B50, 'sc_get.c'),
    ('main', 0x00266D78, 'sdv.c'),
    ('main', 0x002717C0, 'm_math.c'),
    ('main', 0x002965C0, 'xrg_rand_int.c'),
    ('main', 0x001B74A0, 'nml_packet_add.c'),
    ('main', 0x0019F260, 'xgl_1.c'),
    ('ov01', 0x00A811F8, 'obj.c'),
    ('ov01', 0x00A840F0, 'unit_cmd.c'),
    ('ov01', 0x00AA1BE0, 'gr_gp_init.c'),
    ('ov01', 0x00AA2D18, 'menu.c'),
    ('ov01', 0x00AA9340, 'debug_entry.c'),
    ('ov02', 0x00A80000, 'logo.c'),
    ('ov02', 0x00A80798, 'title.c'),
    ('ov02', 0x00A81C78, 'title_hdd_install.c'),
    ('main', 0x001D7250, 'game_pause_disp.c'),  # main/tu049
    ('main', 0x001D9280, 'game_camera.c'),  # main/tu051
    ('main', 0x001E9740, 'yajima_test.c'),  # main/tu060
    # YajimaTest and its helpers (old main/tu063) are part of yajima_test.c (JOINED_CUTS 0x1EDA58); the X1 file
    # tya_display_setting.c (DrawImage, tyaDisplaySetting) is main/tu065, and X1 game_over.c is main/tu059 (2026-11-20)
    ('main', 0x001EE618, 'tya_display_setting.c'),  # main/tu065
    ('main', 0x001E8430, 'game_over.c'),  # main/tu059
    ('main', 0x0020C9A0, 'game_over_095.c'),  # main/tu095: picture helpers of the menu, kept under its old name
    ('main', 0x001F8160, 'get_ladder.c'),  # main/tu075
    ('main', 0x001F18F0, 'runtime.c'),  # main/tu068: X1 runtime.c (the runtime natives)
    ('main', 0x00203118, 'enemy_1.c'),  # main/tu085
    ('main', 0x00206D08, 'enemy_system_init.c'),  # main/tu087
    ('ov01', 0x00A83D28, 'data_unit_org_get.c'),  # ov01/tu009
    ('ov01', 0x00A88E70, 'battle_init.c'),  # ov01/tu022
    ('ov01', 0x00A8A8B8, 'calc.c'),  # ov01/tu023
    ('ov01', 0x00AA8590, 'entry_first_init.c'),  # ov01/tu064
    ('ov01', 0x00AAAB80, 'yamamoto_test.c'),  # ov01/tu074
    ('ov01', 0x00AAAD58, 'm_gs.c'),  # ov01/tu075
    ('ov01', 0x00AAB888, 'm_ef.c'),  # ov01/tu077
    ('ov01', 0x00AB0A38, 'm_ef_create_smp_01.c'),  # ov01/tu083
]

# File names by role for TUs with no single X1 file (2026-11-20, user order: name every tuNNN file, X1 homologs
# first): the subsystem their functions implement, from their names and callers. (unit, first function, name).
ROLE_FILE_NAMES = [
    ('main', 0x00194D08, 'xgl_debug_file.c'),  # main/tu005
    ('main', 0x0019DB58, 'xgl_mc_request.c'),  # main/tu012
    ('main', 0x001A01D8, 'movie_player.c'),  # main/tu017
    ('main', 0x001A3110, 'se_volume.c'),  # main/tu020
    ('main', 0x001A3248, 'se_bank_load.c'),  # main/tu021
    ('main', 0x001A3FA8, 'sound_driver.c'),  # main/tu022
    ('main', 0x001A4768, 'xgl_intc.c'),  # main/tu023
    ('main', 0x001A4A58, 'xgl_dma_handler.c'),  # main/tu024
    ('main', 0x001A6888, 'assert_fail.c'),  # main/tu026
    ('main', 0x001A69A0, 'lan_usb.c'),  # main/tu027
    ('main', 0x001B75E8, 'cur_matrix_asm.c'),  # main/tu039
    ('main', 0x001B7944, 'model_calc_clip.c'),  # main/tu040
    ('main', 0x001B7CD4, 'pxy_profile.c'),  # main/tu041
    ('main', 0x001B8B18, 'pxy_debug_draw.c'),  # main/tu042
    ('main', 0x001CCAA0, 'pxy_packet_state.c'),  # main/tu045
    ('main', 0x001D4568, 'pxy_frame_alpha.c'),  # main/tu047
    ('main', 0x001D5440, 'game_data.c'),  # main/tu048
    ('main', 0x001D74E0, 'game_map_object.c'),  # main/tu050
    ('main', 0x001DEAE8, 'uwamono_light_blend.c'),  # main/tu052
    ('main', 0x001EF600, 'uwamono_objx.c'),  # main/tu067
    ('main', 0x001F4F28, 'runtime_enepc.c'),  # main/tu072
    ('main', 0x001F6F10, 'runtime_uwamono_door.c'),  # main/tu074
    ('main', 0x001F9CE8, 'runtime_camera.c'),  # main/tu077
    ('main', 0x001FC2D8, 'runtime_math.c'),  # main/tu078
    ('main', 0x001FC580, 'runtime_menu.c'),  # main/tu079: the natives of the "menu" module (ptInit .. menuRegisterNatives)
    ('main', 0x001FCBF8, 'uwamono_record_init.c'),  # main/tu082: menuSceneSetupHook, uwamonoInitRecord
    ('main', 0x001FD348, 'uwamono_record.c'),  # main/tu083
    ('main', 0x0020B5C0, 'xgl_cd_arena.c'),  # main/tu092
    ('main', 0x0020BB30, 'gr_screen.c'),  # main/tu093
    ('main', 0x0020C0E0, 'bg_layer.c'),  # main/tu094
    ('main', 0x0020CCC0, 'xgl_tick.c'),  # main/tu096
    ('main', 0x0020CE40, 'font_obj.c'),  # main/tu097
    ('main', 0x0020ED90, 'cursor_buffer.c'),  # main/tu098
    ('main', 0x0020F230, 'text_box.c'),  # main/tu099
    ('main', 0x00210268, 'menu_scene.c'),  # main/tu100
    ('main', 0x0021A520, 'menu_draw.c'),  # main/tu101
    ('main', 0x00228520, 'save_party_init.c'),  # main/tu108
    ('main', 0x00228B88, 'menu_selection.c'),  # main/tu109
    ('main', 0x0022A560, 'pt_member.c'),  # main/tu110
    ('main', 0x0022D170, 'talk_natives.c'),  # main/tu111: the natives of the "talk", "g2path" and "seg" modules (talkCheck .. talkSetResult)
    ('main', 0x0022DEF8, 'render_sprite.c'),  # main/tu115
    ('main', 0x0022E940, 'quest_flag_set.c'),  # main/tu116
    ('main', 0x00233D50, 'txd_table_entry.c'),  # main/tu121
    ('main', 0x00233D78, 'battle_result.c'),  # main/tu122
    ('main', 0x002353B8, 'text_window.c'),  # main/tu123
    ('main', 0x002356B8, 'choice_menu.c'),  # main/tu124
    ('main', 0x00236340, 'text_marker.c'),  # main/tu125
    ('main', 0x00237830, 'save_screen.c'),  # main/tu126
    ('main', 0x0023C600, 'debug_test.c'),  # main/tu127
    ('main', 0x0023EC70, 'xgl_vu1_regs.c'),  # main/tu130
    ('main', 0x0023EDA0, 'xgl_matrix_axes.c'),  # main/tu131
    ('main', 0x0023EFC8, 'xgl_packet_unpack.c'),  # main/tu132
    ('main', 0x0023F8E8, 'ti_wat_sim_update.c'),  # main/tu133
    ('main', 0x00247518, 'cfe_entry.c'),  # main/tu138
    ('main', 0x0024E9A0, 'cfe_init.c'),  # main/tu141
    ('main', 0x0024F3B8, 'yg2_text.c'),  # main/tu143
    ('main', 0x0024F4F0, 'cfe_list.c'),  # main/tu144
    ('main', 0x0024FA48, 'cfe_track.c'),  # main/tu145
    ('main', 0x00250FD8, 'cfe_node_pool.c'),  # main/tu146
    ('main', 0x00251148, 'msg_balance.c'),  # main/tu147
    ('main', 0x002511C0, 'msg_state.c'),  # main/tu148
    ('main', 0x00251B80, 'msg_window_bind.c'),  # main/tu149
    ('main', 0x00251F78, 'vu0_vector.c'),  # main/tu151
    ('main', 0x002520C0, 'vu0_matrix.c'),  # main/tu152
    ('main', 0x002522C8, 'vu0_rotate.c'),  # main/tu153
    ('main', 0x00252680, 'm_math_geom.c'),  # main/tu154
    ('main', 0x00252D88, 'sv_scene_flags.c'),  # main/tu155
    ('main', 0x00252F78, 'sef_light_set.c'),  # main/tu156
    ('main', 0x002594E8, 'draw_list.c'),  # main/tu162
    ('main', 0x00259628, 'sef_entry_counts.c'),  # main/tu163
    ('main', 0x002669F0, 'srs_file.c'),  # main/tu179
    ('main', 0x00272F38, 'm_se_obj.c'),  # main/tu197
    ('main', 0x002733C0, 'fade_ctrl.c'),  # main/tu198
    ('main', 0x00273D28, 'script_load.c'),  # main/tu200
    ('main', 0x00274368, 'gs_make_tex0.c'),  # main/tu204
    ('main', 0x00274650, 'gs_make_frame.c'),  # main/tu205
    ('main', 0x00274738, 'xgl_packet_scratch.c'),  # main/tu206
    ('main', 0x00274830, 'record_render.c'),  # main/tu207
    ('main', 0x00277720, 'record_draw_strip.c'),  # main/tu208
    ('main', 0x00277F78, 'record_turn_angle.c'),  # main/tu209
    ('main', 0x002780E8, 'circle_draw.c'),  # main/tu210
    ('main', 0x00278CF0, 'quad_draw.c'),  # main/tu211
    ('main', 0x0027A6F0, 'sprite_batch_draw.c'),  # main/tu212
    ('main', 0x0027A960, 'gs_queue.c'),  # main/tu213
    ('main', 0x0027BFD0, 'effect_request.c'),  # main/tu214
    ('main', 0x0027C930, 'msg_window_run.c'),  # main/tu215
    ('main', 0x0027CAA0, 'msg_window_text.c'),  # main/tu216
    ('main', 0x0027CBB0, 'msg_window_slot.c'),  # main/tu217
    ('main', 0x0027CC58, 'msg_window_open.c'),  # main/tu218
    ('main', 0x0027CD98, 'msg_window_move.c'),  # main/tu219
    ('main', 0x0027CF90, 'msg_window_close.c'),  # main/tu220
    ('main', 0x0027D2F8, 'msg_window_advance.c'),  # main/tu221
    ('main', 0x0027D438, 'msg_window_scroll.c'),  # main/tu222
    ('main', 0x0027D918, 'timecode_choice.c'),  # main/tu223
    ('main', 0x0027DA20, 'timecode_hms.c'),  # main/tu224
    ('main', 0x0027DCB0, 'msg_slot_start.c'),  # main/tu225
    ('main', 0x0027E148, 'interp_text.c'),  # main/tu226
    ('main', 0x0027E1B8, 'interp_update.c'),  # main/tu227
    ('main', 0x0027E600, 'talk_scan.c'),  # main/tu228
    ('main', 0x0027E7E0, 'talk_script.c'),  # main/tu229
    ('main', 0x002803D8, 'talk_window_frame.c'),  # main/tu230
    ('main', 0x00280728, 'talk_draw_fade.c'),  # main/tu231
    ('main', 0x00281A90, 'talk_text_box.c'),  # main/tu232
    ('main', 0x00281BE0, 'talk_speaker_name.c'),  # main/tu233
    ('main', 0x00281CA0, 'msg_text_draw.c'),  # main/tu234
    ('main', 0x00284300, 'msg_list_slot.c'),  # main/tu235
    ('main', 0x002843D0, 'msg_window_layout.c'),  # main/tu236
    ('main', 0x00284590, 'msg_window_center.c'),  # main/tu237
    ('main', 0x00284620, 'msg_window_update.c'),  # main/tu238
    ('main', 0x00285048, 'msg_window_size.c'),  # main/tu239
    ('main', 0x002850B8, 'timecode_window.c'),  # main/tu240
    ('main', 0x00285910, 'timecode_frames.c'),  # main/tu241
    ('main', 0x002860D0, 'msg_slot_core.c'),  # main/tu242
    ('main', 0x00286ED0, 'ease_step.c'),  # main/tu243
    ('main', 0x00286F50, 'msg_slot_center.c'),  # main/tu244
    ('main', 0x00287120, 'text_tag.c'),  # main/tu245
    ('main', 0x00287A08, 'xgl_font_width.c'),  # main/tu246
    ('main', 0x0028B410, 'xeno_actor_set.c'),  # main/tu248
    ('main', 0x00292D70, 'xgl_vector_ops.c'),  # main/tu254
    ('main', 0x00292F00, 'xgl_cur_matrix.c'),  # main/tu255
    ('main', 0x00293178, 'xgl_cur_mul.c'),  # main/tu256
    ('main', 0x00295260, 'xac_eval.c'),  # main/tu259
    ('main', 0x00297168, 'ti_dynamics_node.c'),  # main/tu263
    ('main', 0x0029ACE0, 'ti_dynamics_coll_push.c'),  # main/tu270
    ('main', 0x0029C288, 'undulate.c'),  # main/tu272
    ('main', 0x0029E6A0, 'map_scan_node.c'),  # main/tu273
    ('ov01', 0x00A80000, 'ov01_slot.c'),  # ov01/tu000
    ('ov01', 0x00A801E0, 'ov01_task.c'),  # ov01/tu001
    ('ov01', 0x00A80368, 'mons_slot_run.c'),  # ov01/tu002
    ('ov01', 0x00A804D8, 'unit_side_table.c'),  # ov01/tu003
    ('ov01', 0x00A80660, 'data_obj_table.c'),  # ov01/tu004
    ('ov01', 0x00A807E8, 'obj_slot.c'),  # ov01/tu005
    ('ov01', 0x00A80E40, 'gr_matrix.c'),  # ov01/tu006
    ('ov01', 0x00A818E8, 'data_file_load.c'),  # ov01/tu008
    ('ov01', 0x00A87E38, 'unit_tbl.c'),  # ov01/tu017
    ('ov01', 0x00A88348, 'unit_fade.c'),  # ov01/tu018
    ('ov01', 0x00A88A50, 'mons_slot.c'),  # ov01/tu019
    ('ov01', 0x00A88C60, 'mons_slot_show.c'),  # ov01/tu020
    ('ov01', 0x00A88CB0, 'mons_slot_fade.c'),  # ov01/tu021
    ('ov01', 0x00A92150, 'cmd_exec.c'),  # ov01/tu033
    ('ov01', 0x00A97110, 'event_stock.c'),  # ov01/tu034
    ('ov01', 0x00A97180, 'tgt_select.c'),  # ov01/tu035
    ('ov01', 0x00A97D68, 'choice_menu.c'),  # ov01/tu036
    ('ov01', 0x00A99EC0, 'scene.c'),  # ov01/tu037
    ('ov01', 0x00A9A4B0, 'scene_load.c'),  # ov01/tu038
    ('ov01', 0x00A9AF68, 'scene_finish.c'),  # ov01/tu039
    ('ov01', 0x00A9B0F8, 'scene_script.c'),  # ov01/tu040
    ('ov01', 0x00A9B120, 'effect_pos.c'),  # ov01/tu041
    ('ov01', 0x00A9B280, 'data_record_get.c'),  # ov01/tu042
    ('ov01', 0x00A9C610, 'data_box.c'),  # ov01/tu043
    ('ov01', 0x00A9C928, 'sb_vm.c'),  # ov01/tu044
    ('ov01', 0x00A9EE28, 'sb_push_const.c'),  # ov01/tu045
    ('ov01', 0x00A9F0A8, 'sb_natives.c'),  # ov01/tu046
    ('ov01', 0x00A9FF80, 'sb_natives_unit.c'),  # ov01/tu047
    ('ov01', 0x00AA5248, 'act_step_icon.c'),  # ov01/tu055
    ('ov01', 0x00AA5390, 'menu_wep_sp_panel.c'),  # ov01/tu056
    ('ov01', 0x00AA6148, 'menu_unit_panel.c'),  # ov01/tu058
    ('ov01', 0x00AA6390, 'menu_unit_panel_open.c'),  # ov01/tu059
    ('ov01', 0x00AA64A8, 'menu_icon_list.c'),  # ov01/tu060
    ('ov01', 0x00AA6BF8, 'menu_combo_panel.c'),  # ov01/tu061
    ('ov01', 0x00AA70E8, 'menu_panel.c'),  # ov01/tu062
    ('ov01', 0x00AA8200, 'menu_tex_state.c'),  # ov01/tu063
    ('ov01', 0x00AA8930, 'deco_flags.c'),  # ov01/tu066
    ('ov01', 0x00AA8B70, 'screen_flash.c'),  # ov01/tu067
    ('ov01', 0x00AA8C10, 'screen_fill.c'),  # ov01/tu068
    ('ov01', 0x00AA8EA0, 'handle_table.c'),  # ov01/tu069
    ('ov01', 0x00AA9058, 'handle_state.c'),  # ov01/tu070
    ('ov01', 0x00AA90C8, 'snd_play.c'),  # ov01/tu071
    ('ov01', 0x00AAB010, 'm_ef_path.c'),  # ov01/tu076
    ('ov01', 0x00AADBE0, 'm_ef_sway.c'),  # ov01/tu078
    ('ov01', 0x00AAE748, 'm_ef_missile_trail.c'),  # ov01/tu079
    ('ov01', 0x00AAEEE8, 'm_ef_spiral.c'),  # ov01/tu080
    ('ov01', 0x00AAF718, 'm_ef_thunder25.c'),  # ov01/tu081
    ('ov01', 0x00AB0088, 'm_ef_thunder31.c'),  # ov01/tu082
    ('ov01', 0x00AB14E0, 'm_ef_flag_window.c'),  # ov01/tu084
    ('ov01', 0x00AB1CE0, 'm_ef_wave.c'),  # ov01/tu085
    ('ov01', 0x00AB2750, 'm_ef_spray.c'),  # ov01/tu086
    ('ov01', 0x00AB3210, 'm_ef_actor_path.c'),  # ov01/tu088
    ('ov01', 0x00AB3CC8, 'm_ef_actor_history.c'),  # ov01/tu089
]


def x1_file_name(unit, start, suggestion):
    """A TU's name from SPLIT_CUTS / X1_FILE_NAMES / ROLE_FILE_NAMES when its first function is listed, else the suggestion."""
    wanted = {(u, va): n for u, va, n in X1_FILE_NAMES}
    wanted.update({(u, va): n for u, va, _, n, _ in SPLIT_CUTS})
    name = wanted.get((unit.image.unit, start))
    if name:
        return dict(name=name, confidence='medium', basis='X1 file of its homologs (X1_FILE_NAMES / SPLIT_CUTS)')
    name = {(u, va): n for u, va, n in ROLE_FILE_NAMES}.get((unit.image.unit, start))
    if name:
        return dict(name=name, confidence='medium', basis='named by its role (ROLE_FILE_NAMES)')
    return suggestion

def build_units(mean_functions, model_file, homologs, unit, zones, o0_homologs=None):
    """TU list of one image: inferred cuts inside game zones, one coarse record per other zone."""
    model = model_file['model']
    rows = unit.features()
    odds = score(model, rows)
    tus = []
    stats = collections.Counter()
    ordinal = 0
    for zone in zones:
        indices = [i for i, va in enumerate(unit.vas) if zone['start'] <= va < zone['end']]
        if not indices:
            continue
        if zone['cls'] != 'game':
            tus.append(dict(_indices=indices, _zone=zone, _cut=None))
            continue
        in_scope = set(indices[:-1])
        log_odds = [value if gap in in_scope else OUT_OF_SCOPE for gap, value in enumerate(odds)]
        blocked = unit.hard_no_cut()
        keep, homolog_cuts = homolog_constraints(unit, homologs, set(indices))
        file_cuts = unit.forced_cuts()
        for a, b in keep:
            conflict = any(low >= a and high < b for low, high, _, _ in file_cuts)
            stats['homolog_keep_spans'] += 1
            if conflict:
                stats['homolog_keep_vs_file_conflicts'] += 1
                continue
            for gap in range(a, b):
                blocked[gap] = True
        o0_keep, o0_cuts = o0_homolog_constraints(unit, o0_homologs or {}, set(indices))
        for a, b in o0_keep:
            stats['o0_homolog_keep_spans'] += 1
            for gap in range(a, b):
                blocked[gap] = True
        # A .rodata jump inside such a span is a lead against whole compiled bodies, so it gives way.
        forced = []
        for item in file_cuts:
            low, high, kind, _ = item
            whole = low in in_scope and high in in_scope
            if kind in ('dup_string', 'lit4_step', 'rodata_pad') and not whole:
                continue
            if not (low in in_scope or high in in_scope):
                continue
            if kind == 'rodata_order' and any(low >= a and high < b for a, b in o0_keep):
                stats['rodata_order_inside_o0_span'] += 1
                continue
            if kind in ('dup_string', 'lit4_step', 'rodata_pad') and all(blocked[g] for g in range(low, high + 1)):
                stats['small_sample_cut_gives_way'] += 1   # the no-cut rule has far more evidence behind it
                continue
            forced.append(item)
        forced += homolog_cuts + o0_cuts
        target = max(1, round(len(indices) / mean_functions))
        cuts = segment(log_odds, blocked, forced, target)
        for start, moved_to, note in MOVED_CUTS:
            if start in unit.index and moved_to in unit.index and unit.index[start] - 1 in cuts:
                del cuts[unit.index[start] - 1]
                cuts[unit.index[moved_to] - 1] = [('compiler', note)]
        for low, high, kind, note in file_cuts:
            if low in in_scope:
                holds = any(not blocked[g] for g in range(low, high + 1))
                stats['file_intervals_with_free_gap' if holds else 'file_intervals_all_blocked'] += 1
        stats['forced_intervals'] += len(forced)
        for kind_list in cuts.values():
            stats['cuts_' + (kind_list[0][0] if kind_list else 'model')] += 1
        # Merge only: a cut a shared literal or a joined start rules out is dropped after
        # segmentation, so no other cut moves and every other TU keeps its ordinal.
        zone_set = set(indices)
        joined = {unit.index[va] - 1: why for va, why in JOINED_CUTS if va in unit.index}
        merges = {}
        for gap in cuts:
            if gap in joined:
                merges[gap] = 'joined: ' + joined[gap]
                continue
            for low, high, target_va, value in unit.text_spans:
                if low <= gap < high and low in zone_set and high in zone_set:
                    merges[gap] = (f'literal {value[:40].decode("latin-1")!r} at {hex32(target_va)} is used by '
                                   f'{hex32(unit.vas[low])} and {hex32(unit.vas[high])}')
                    break
        stats['cuts_merged'] += len(merges)
        ordered = sorted(cuts)
        edges = [indices[0]] + [gap + 1 for gap in ordered] + [indices[-1] + 1]
        for position in range(len(edges) - 1):
            tu_indices = list(range(edges[position], edges[position + 1]))
            gap = ordered[position - 1] if position else None
            tus.append(dict(_indices=tu_indices, _zone=zone, _cut=gap, _cuts=cuts, _odds=log_odds,
                            _merge=merges.get(gap) if gap is not None else None))
    for tu in tus:
        tu['ordinal'] = ordinal
        tu['data'] = {}
        ordinal += 1
    merged = []
    for tu in tus:
        if tu.get('_merge') and merged:
            head = merged[-1]
            head['_indices'] = head['_indices'] + tu['_indices']
            head.setdefault('_merged', []).append(dict(ordinal=tu['ordinal'], start=hex32(unit.vas[tu['_indices'][0]]),
                                                       reason=tu['_merge']))
            if unit.vas[tu['_indices'][0]] in JOINED_KEEPS_LATER_ORDINAL:
                head['ordinal'] = tu['ordinal']
            continue
        merged.append(tu)
    # Split only: a SPLIT_CUTS start opens a new TU with its own fixed ordinal, after every merge, so no
    # other TU changes ordinal (the TU list stays in address order; data attribution uses that order).
    splits = {va: (ordinal, why) for name, va, ordinal, _, why in SPLIT_CUTS if name == unit.image.unit}
    result = []
    for tu in merged:
        current = tu
        result.append(current)
        for i in tu['_indices'][1:]:
            if unit.vas[i] in splits:
                ordinal, why = splits[unit.vas[i]]
                at = current['_indices'].index(i)
                piece = dict(current, _indices=current['_indices'][at:], ordinal=ordinal, data={}, _cut=None,
                             _merge=None, _split=why)
                piece.pop('_merged', None)
                current['_indices'] = current['_indices'][:at]
                result.append(piece)
                current = piece
                stats['cuts_split'] += 1
    return result, stats


def render_main_tu(unit, tu, names, homologs, model_file):
    image = unit.image
    indices = tu['_indices']
    zone = tu['_zone']
    first = unit.functions[indices[0]]
    last = unit.functions[indices[-1]]
    # A coarse library record covers its whole zone, including bytes before its first function.
    start = max(first['va'], zone['start']) if tu['ordinal'] and zone['cls'] == 'game' else zone['start']
    next_va = tu.get('_next_start', last['end'])
    end = next_va
    in_game = zone['cls'] == 'game'
    if in_game:
        evidence, confidence = boundary_record(tu.get('_cuts', {}), tu['_cut'], tu.get('_odds'), model_file)
        if tu.get('_split'):
            evidence, confidence = [f'split: {tu["_split"]}'], 'medium'
        name, files = suggest_name(unit, indices, names, homologs, tu['ordinal'])
        name = x1_file_name(unit, first['va'], name)
    else:
        evidence, confidence = [f'coarse zone: {zone["evidence"]}'], 'zone'
        name, files = dict(name=zone['stem'] + '.s', confidence='low', basis='zone label, not an object'), []
    size = end - start
    return dict(
        id=f'main/tu{tu["ordinal"]:03d}', ordinal=tu['ordinal'], start=hex32(start), end=hex32(end),
        code_end=hex32(last['end']), size=size, file_offset_start=start + image.base, file_offset_end=end + image.base,
        start_alignment=(start & -start) if start else 0, function_count=len(indices),
        functions=function_table(unit, names, indices),
        boundary_kind='inferred' if in_game else 'coarse_zone', boundary_evidence=evidence, boundary_confidence=confidence,
        classification=dict(cls=zone['cls'], confidence='high' if in_game else 'medium', evidence=[zone['evidence']],
                            library_families=zone.get('families', {})),
        in_scope=in_game, suggested_name=name, source_file_strings=files,
        optimization=('-O0' if image.in_o0_zone(first['va']) else '-O2') if in_game else None,
        x1_homologs=sorted({homologs[unit.vas[i]][1] for i in indices if unit.vas[i] in homologs}),
        accepted=dict(count=0, functions=[], unit_records=[]), data=tu['data'],
        legacy_group_hint={}, static_symbols_by_section={},
        provisional=dict(reason='inferred without symbols; see boundary_evidence and reports/x2-tu-map.md'),
        **(dict(merged_from=tu['_merged']) if tu.get('_merged') else {}))


def render_overlay_tu(unit, unit_name, tu, names, homologs, model_file):
    image = unit.image
    indices = tu['_indices']
    start = unit.functions[indices[0]]['va']
    end = tu.get('_next_start', unit.functions[indices[-1]]['end'])
    evidence, confidence = boundary_record(tu.get('_cuts', {}), tu['_cut'], tu.get('_odds'), model_file)
    if tu.get('_split'):
        evidence, confidence = [f'split: {tu["_split"]}'], 'medium'
    name, files = suggest_name(unit, indices, names, homologs, tu['ordinal'])
    name = x1_file_name(unit, start, name)
    stem = re.sub(r'\.(c|s)$', '', name['name'])
    # Overlay files are named by ordinal, except a TU whose name is listed (X1_FILE_NAMES / SPLIT_CUTS / ROLE_FILE_NAMES).
    listed_name = stem if name['basis'].startswith(('X1 file of its homologs (X1_FILE_NAMES',
                                                       'named by its role (ROLE_FILE_NAMES)')) else None
    data = []
    for family, row in tu['data'].items():
        data.append(dict(family='unsplit', start_window=[row['lower_bound_start'], row['anchored_start']],
                         end_window=[row['anchored_end'], row['upper_bound_end']], anchors=row['anchors']))
    text = image.data[start + image.base:end + image.base]
    return dict(
        ordinal=tu['ordinal'], id=f'{unit_name}/tu{tu["ordinal"]:03d}_{start:08x}', text=dict(
            start=hex32(start), end=hex32(end), size=end - start, file_offset=start + image.base,
            file_offset_end=end + image.base, sha256=hashlib.sha256(text).hexdigest()),
        text_alignment_of_start=(start & -start) if start else 0, text_tail_padding=end - unit.functions[indices[-1]]['end'],
        boundary_evidence=dict(start='; '.join(evidence), end='next inferred cut', empty_text=False),
        boundary_confidence=confidence, language_marker='c', classification='game',
        classification_basis='overlay code; the whole section is game code', function_count=len(indices),
        global_function_count=None, functions=function_table(unit, names, indices), local_data_symbols=[],
        data_contributions=data, source_file_strings=files, suggested_name=stem,
        suggested_path=f'src/{unit_name}/{stem}.c', suggested_name_basis=name['basis'],
        suggested_name_confidence=name['confidence'], **({'file_name': listed_name} if listed_name else {}),
        accepted_functions=0, accepted_unit_records=[],
        accepted_sources=[], accepted_function_list=[], provisional_tus={}, names_also_defined_in_main=[],
        global_data=[], subsystem=None, in_scope=True,
        x1_homologs=sorted({homologs[unit.vas[i]][1] for i in indices if unit.vas[i] in homologs}))


# The `.rodata` output section inside an overlay's data half: (first byte, why). The overlay headers merge
# every section into one, so the edge is read from the data: references from code restart the TU order where
# one output section ends and the next begins (.data, .rodata, .bss, each in link order). The section runs to
# the `.bss` start (the 8-aligned zero tail), and each TU's piece is cut as main's data families are
# (x2_scaffold.data_cuts): at its first address only that TU references, the first piece at the section start.
OVERLAY_RODATA = {
    'ov01': (0x00AB8CA0,
             'the code references of the data half increase by TU from tu008 (0xAB4500) to tu082 (0xAB8C50-0xAB8C9F, '
             'floats), then restart at tu008 with the string pool 0xAB8CA0.. that the pointer table of tu008\'s .data '
             '(0xAB4500..) names and its jump table 0xABA210, and increase again through the 24 switch jump tables '
             '0xABA210-0xABA8F0 and the literals of tu047-tu090 up to 0xABB870, where .bss restarts at tu000; '
             'found banking unitCmdCodeGet (ov01/tu013), whose C jump table had no piece to land in, 2026-11-17'),
    'ov02': (0x00A83178,
             'the code references of the data half increase by TU from tu000 (0xA82850) to tu002 (0xA83150-0xA83160, '
             'literals; the pointer table 0xA83168-0xA83174 is tu002 .data), then restart at tu000 with the string '
             'pool 0xA83178.. and increase through tu001 and tu002 (jump tables 0xA83750, 0xA837B0, 0xA83C30) up to '
             '0xA83C48, where .bss restarts; found banking HddInstallCB and TitleHddInstall (ov02/tu002), whose C jump '
             'tables had no piece to land in, 2026-11-18'),
}


# Code references to an overlay's `.rodata` that scan_refs does not pair: (target, function VA, why). They take
# part in the piece cuts like the scanned ones.
OVERLAY_RODATA_REFERENCES = {
    'ov01': (
        (0x00ABB7D0, 0x00AB3668,
         'MEfDrawPathTrail: `lui $3, %hi(0xABB7D0)` sits in the delay slot of the parity `bnez` at 0xAB37FC and its '
         '`addiu $3, $3, %lo` at 0xAB3814 follows the branch target label, so the pair spans two blocks; the two '
         'colour quadwords 0xABB7D0-0xABB7F0 are read by no other function (2026-11-20)'),
        (0x00ABB7F0, 0x00AB3668,
         'MEfDrawPathTrail: `lui $2, %hi(0xABB7F0)` is in the delay slot of the `b` at 0xAB39C8 and again at '
         '0xAB39D8, the `addiu $30, $2, %lo` at 0xAB39E4 follows the join label (2026-11-20)'),
        # A function-scope `static const` 128-bit colour is emitted at its declaration and loaded as an
        # immediate (li/dsll32/ori/pcpyld), so no instruction references the datum and it would fall to the
        # previous file's piece. Each quadword below equals the immediate its function builds (2026-11-20).
        (0x00ABB510, 0x00AAF2D0,
         'func_00AAF2D0: the outer colour {0,0x40,0x80,0xC0} at 0xABB510 and the inner colour {0,0,0x80,0x30} '
         'at 0xABB520 are the two immediates of its strips'),
        (0x00ABB530, 0x00AAFA50,
         'func_00AAFA50: the half-alpha grey {0x80,0x80,0x80,0x40} is the immediate of its quad; it precedes the '
         'texture names of the functions that follow it in the file'),
        (0x00ABB570, 0x00AB04D0,
         'func_00AB04D0: the same half-alpha grey, the twin quad function of the next file'),
        (0x00ABB690, 0x00AB18F8,
         'MEfDrawFlagWindow: the strip colour {0x40,0,0,0xFF} is its immediate; the file before it '
         '(MEfDrawRotatedQuads) never reads it'),
    ),
}


def overlay_rodata_pieces(unit, tus, low, high):
    """[(ordinal, start, end, anchors)] of the `.rodata` section [low, high) of an overlay, and its TU order."""
    owner = {i: tu['ordinal'] for tu in tus for i in tu['_indices']}
    users = collections.defaultdict(set)
    for table in (unit.users, unit.indexed_users):
        for target, who in table.items():
            if low <= target < high:
                users[target] |= who
    index_of = {va: i for i, va in enumerate(unit.vas)}
    for target, function, _ in OVERLAY_RODATA_REFERENCES.get(unit.image.unit, ()):
        if low <= target < high:
            users[target].add(index_of[function])
    claims = collections.defaultdict(list)
    for target, who in users.items():
        ordinals = {owner[i] for i in who if i in owner}
        if len(ordinals) == 1 and all(i in owner for i in who):
            claims[next(iter(ordinals))].append(target)
    cuts = sorted(((min(targets) & ~3, ordinal) for ordinal, targets in claims.items()))
    rank = {tu['ordinal']: position for position, tu in enumerate(tus)}   # address order (SPLIT_CUTS ordinals)
    order = [rank[ordinal] for _, ordinal in cuts]
    monotonic = order == sorted(order)
    if cuts:
        cuts[0] = (low, cuts[0][1])
    pieces = [(ordinal, start, cuts[k + 1][0] if k + 1 < len(cuts) else high, len(claims[ordinal]))
              for k, (start, ordinal) in enumerate(cuts)]
    return pieces, monotonic


# The `.data` and `.bss` output sections of an overlay's data half are cut per TU like `.rodata`, but by the
# link order rule of main's data map (cross-section consistency, 2026-11-17): every section of one object
# follows the same order as its `.text`, so the TU pieces are the longest chain of single-TU code references
# whose TU ordinals never decrease with the address. A reference outside that chain is a global of the TU whose
# piece holds it, read by another file (ov01 .bss: 0xABFF20 read by tu009 inside tu005's piece, the 0xAC0010
# buffer read by tu036 inside tu008's, 0xAE0E08 read by tu056 inside tu048's; data pass 7, 2026-11-19).
OVERLAY_DATA_SECTIONS = ('.data', '.bss')

# References to an inner address of an object that another TU defines (a field or an element view read through
# its own label): not a piece start. Each one was cut out of its object by the chain above (data pass 7,
# 2026-11-19).
OVERLAY_INNER_REFERENCES = [
    ('ov01', 0x00AC104C,
     'unitTbl+0xC, read by tu017: unitTbl is the 32-byte slot table of tu010 (unitInit memsets 0x20 bytes from '
     '0xAC1040, tu017 indexes 8 slots); the cut at 0xAC104C gave tu010 12 bytes and tu017 the 20-byte tail'),
    ('ov01', 0x00AE0F20,
     'handleSlots+8, the state word of the 12-byte HandleSlot records that tu070 reads as D_00AE0F20[i]: '
     'handleSlots (0xAE0F18, 6 records, 72 bytes) and handleBumpCursor (0xAE0F60) are tu069\'s (handleInit '
     'memsets sizeof(handleSlots) and sets the cursor)'),
]

# References whose address lies in front of the object they index (`table[i - 1]` folds the -1 into the address):
# the object, and so the piece of its TU, starts at the second address (data pass 7, 2026-11-19).
OVERLAY_BIASED_REFERENCES = [
    ('ov01', 0x00AB6AB4, 0x00AB6AB8,
     'bgmTrackHandles[track - 1] of bgmPlayTrack (tu071, tracks 1 to 6): the table of six file names starts at '
     '0xAB6AB8 (alignment 8); 0xAB6AB4 is the ALPHA high word of tu063\'s 16-byte menuStatusIconsTexRecord'),
]


def overlay_link_order_pieces(unit, unit_name, tus, low, high):
    """([(ordinal, start, end, anchors)], [(target, ordinal) outside the chain]) of [low, high) of an overlay."""
    owner = {i: tu['ordinal'] for tu in tus for i in tu['_indices']}
    rank = {tu['ordinal']: position for position, tu in enumerate(tus)}   # address order (SPLIT_CUTS ordinals)
    users = collections.defaultdict(set)
    for table in (unit.users, unit.indexed_users):
        for target, who in table.items():
            if low <= target < high:
                users[target] |= who
    # Every reference is a candidate (target, ordinal of one reader); a single-reader one weighs 2, a shared
    # one 1, so a shared global anchors a TU only where that reader lies inside the link-order interval.
    candidates = []
    inner = {target for name, target, _ in OVERLAY_INNER_REFERENCES if name == unit_name}
    for name, target, start, _ in OVERLAY_BIASED_REFERENCES:
        if name == unit_name and target in users:
            users[start] |= users.pop(target)
    for target in sorted(users):
        if target in inner:
            continue
        ordinals = {owner.get(i) for i in users[target]} - {None}
        weight = 2 if len(ordinals) == 1 and all(i in owner for i in users[target]) else 1
        candidates += [(target, ordinal, weight) for ordinal in sorted(ordinals, key=rank.get)]
    # Heaviest chain with strictly increasing targets and non-decreasing TU positions (O(n^2), n is small).
    best = [w for _, _, w in candidates]
    back = [-1] * len(candidates)
    for k, (target, ordinal, weight) in enumerate(candidates):
        for j in range(k):
            if candidates[j][0] < target and rank[candidates[j][1]] <= rank[ordinal] and best[j] + weight > best[k]:
                best[k], back[k] = best[j] + weight, j
    chain = []
    k = max(range(len(candidates)), key=lambda i: (best[i], -i)) if candidates else -1
    while k >= 0:
        chain.append(candidates[k][:2])
        k = back[k]
    chain.reverse()
    kept = {target for target, _ in chain}
    outside = [(target, ordinal) for target, ordinal, weight in candidates if weight == 2 and target not in kept]
    starts, counts = [], collections.Counter(ordinal for _, ordinal in chain)
    for target, ordinal in chain:
        if not starts or starts[-1][1] != ordinal:
            starts.append((target & ~3, ordinal))
    if starts:
        starts[0] = (low, starts[0][1])
    pieces = [(ordinal, start, starts[k + 1][0] if k + 1 < len(starts) else high, counts[ordinal])
              for k, (start, ordinal) in enumerate(starts)]
    return pieces, outside


def close_gaps(unit, tus, zones):
    """Each TU runs to the next one's first function, so the records cover the zone with no hole."""
    for position, tu in enumerate(tus):
        following = tus[position + 1] if position + 1 < len(tus) else None
        if following and following['_zone'] is tu['_zone']:
            tu['_next_start'] = unit.functions[following['_indices'][0]]['va']
        else:
            tu['_next_start'] = tu['_zone']['end']


def vu_objects(image):
    """The VU sections of main: in scope by user decision, and not split into programs here.

    A `.DVP.overlay..0x0.` name opens a new assembler object. X1's symbols confirm this
    grouping on its own overlays, and X2 has only the section names.
    """
    overlays = [s for s in sorted(image.sections.values(), key=lambda s: s['index'])
                if s['name'].startswith('.DVP.overlay')]
    groups = []
    for section in overlays:
        if '..0x0.' in section['name'] or not groups:
            groups.append([])
        groups[-1].append(section)
    sections = {}
    for name in ('.vutext', '.vudata'):
        section = image.sections[name]
        sections[name] = dict(va=hex32(section['va']), file_offset=section['offset'], size=section['size'])
    return dict(
        note='VU microcode counts as game code (user, 2026-11-05). Recovered as VU programs '
             '(docs/vu-microcode-unit.md) and reported apart as exact_vu_microcode, never in TU credit.',
        in_scope=True, sections=sections, overlay_bytes=sum(s['size'] for s in overlays),
        vu_microcode_objects=[dict(index=position, bytes=sum(s['size'] for s in group),
                                   dvp_overlays=[s['name'] for s in group]) for position, group in enumerate(groups)])


def generate(root=ROOT):
    model_path = root / MODEL_FILE
    if not model_path.is_file():
        raise SystemExit(f'{MODEL_FILE} is missing: run map_infer.py calibrate first')
    model_file = json.loads(model_path.read_text())
    mean_functions = model_file['mean_functions']
    target = json.loads((root / 'config/targets.json').read_text())
    game_dir = root / 'game/extracted-iso'
    summary = {}

    image = Image(game_dir / 'SLUS_208.92', 'main')
    image.o0_zones = MAIN_O0_ZONES
    unit = Unit(image)
    names, homologs = load_annotations(root / 'annotations/slus_208.92_annotations.csv')
    zones = [dict(zone) for zone in MAIN_ZONES]
    o0_homologs = load_o0_homologs(root)
    tus, stats = build_units(mean_functions, model_file, homologs, unit, zones, o0_homologs)
    for va, row in o0_homologs.items():
        homologs.setdefault(va, row[:2])
    close_gaps(unit, tus, zones)
    section_order = attribute_data(unit, tus)
    records = [render_main_tu(unit, tu, names, homologs, model_file) for tu in tus]
    zone_of = lambda va: next((z for z in zones if z['start'] <= va < z['end']), None)
    wrong = []
    for caller, targets in enumerate(unit.calls):
        zone = zone_of(unit.vas[caller])
        if zone is None or zone['cls'] == 'game':
            continue
        for callee in targets:
            other = zone_of(callee)
            if callee in unit.index and other and other['cls'] == 'game':
                wrong.append((hex32(unit.vas[caller]), hex32(callee)))
    missing_names = sorted(hex32(va) for va in names if va not in unit.index)
    outside_game = sorted(hex32(va) for va in names if va in unit.index and zone_of(va)['cls'] != 'game')
    ov02_section = image.sections.get('ov02')
    ov02_image = Image(game_dir / 'OV02.OVL', 'ov02')
    section_bytes = image.data[ov02_section['offset']:ov02_section['offset'] + ov02_section['size']]
    other_bytes = ov02_image.data[ov02_image.sections['ov02']['offset']:][:ov02_section['size']]
    game_records = [r for r in records if r['in_scope']]
    main_map = dict(
        schema_version=1, analysis_only=True, artifact='X2 main TU map, inferred without symbols',
        generated_by='tools/tu/map_infer.py generate',
        identity=dict(binary='game/extracted-iso/SLUS_208.92', binary_sha256=image.sha256, gp=hex32(image.gp),
                      revision=target['revision'] if 'revision' in target else 'slus-20892-1f978e64c837',
                      section='.text', unit='main'),
        inputs=dict(annotations='annotations/slus_208.92_annotations.csv', model=MODEL_FILE, o0_homologs=O0_HOMOLOGS_FILE),
        method=dict(
            text_boundaries='Inferred: see reports/x2-tu-map.md. X2 has no gcc2_compiled. symbols.',
            classification='Zones checked by the call graph: library zones never call game code, except crt0 to main.',
            data_attribution='Data referenced by exactly one inferred TU; a lead, as the boundaries are inferred.',
            naming='__FILE__ string, then X1 homolog file, then a named function; otherwise none.',
            object_grouping='Out-of-scope zones are single coarse records, not objects.',
            limitations=['Boundary precision is modest; see the report.', 'Function bindings are unknown.']),
        section_order=section_order, tus=records, anomalies=[], non_text_objects=vu_objects(image),
        appendix_ov02_section=dict(va=hex32(ov02_section['va']), size=ov02_section['size'],
                                   file_offset=ov02_section['offset'],
                                   identical_to_ov02_ovl=section_bytes == other_bytes),
        validation=dict(library_zone_calls_into_game=wrong, annotated_starts_not_found=missing_names,
                        annotated_outside_game_zones=outside_game),
        summary=dict(tus=len(records), game_tus=len(game_records),
                     game_functions=sum(r['function_count'] for r in game_records),
                     game_bytes=sum(r['size'] for r in game_records), all_functions=len(unit.vas), stats=dict(stats)))
    write_json(root / '.work/tu-migration/map-main/tu-map-main.json', main_map)
    summary['main'] = main_map['summary']

    overlays = {}
    for unit_name, file_name, annotations in OVERLAYS:
        image = Image(game_dir / file_name, unit_name)
        image.text_end = find_text_end(image)
        unit = Unit(image)
        names, homologs = load_annotations(root / annotations)
        section = image.sections[unit_name]
        zones = [dict(start=section['va'], end=image.text_end, cls='game', evidence='overlay code')]
        tus, stats = build_units(mean_functions, model_file, homologs, unit, zones)
        close_gaps(unit, tus, zones)
        attribute_data(unit, tus)
        records = [render_overlay_tu(unit, unit_name, tu, names, homologs, model_file) for tu in tus]
        tail = section['offset'] + section['size']
        zero_from = tail
        while zero_from > section['offset'] and image.data[zero_from - 1] == 0:
            zero_from -= 1
        body = image.data[section['offset']:tail]
        section_end = section['va'] + section['size']
        bss_start = min((zero_from - section['offset'] + section['va'] + 7) & ~7, section_end)
        families = [dict(family='unsplit', start=hex32(image.text_end), end=hex32(section_end),
                         size=section_end - image.text_end, anchors_kept=0, owners=[], evidence_kinds={})]
        monotonic = None
        if unit_name in OVERLAY_RODATA:
            rodata_start, why = OVERLAY_RODATA[unit_name]
            pieces, monotonic = overlay_rodata_pieces(unit, tus, rodata_start, bss_start)
            ids = {r['ordinal']: r['id'] for r in records}
            data_pieces, data_outside = overlay_link_order_pieces(unit, unit_name, tus, image.text_end, rodata_start)
            bss_pieces, bss_outside = overlay_link_order_pieces(unit, unit_name, tus, bss_start, section_end)

            def linked(family, section_name, low, high, rows, outside):
                return dict(family=family, section=section_name, start=hex32(low), end=hex32(high), size=high - low,
                            anchors_kept=sum(p[3] for p in rows),
                            owners=[dict(ordinal=o, id=ids[o], start=hex32(a), end=hex32(b),
                                         anchors=dict(code_ref_link_order=n)) for o, a, b, n in rows],
                            evidence_kinds=dict(code_ref_link_order=len(rows)),
                            read_by_other_tus=[dict(target=hex32(t), reader=ids[o]) for t, o in outside],
                            evidence='OVERLAY_DATA_SECTIONS: link-order chain of code references (data pass 7, 2026-11-19)')
            families = [
                linked('data', '.data', image.text_end, rodata_start, data_pieces, data_outside),
                dict(family='rodata', section='.rodata', start=hex32(rodata_start), end=hex32(bss_start),
                     size=bss_start - rodata_start, anchors_kept=sum(p[3] for p in pieces),
                     owners=[dict(ordinal=o, id=ids[o], start=hex32(a), end=hex32(b), anchors=dict(code_ref_single_tu=n))
                             for o, a, b, n in pieces],
                     evidence_kinds=dict(code_ref_single_tu=len(pieces)), evidence=why),
                linked('bss', '.bss', bss_start, section_end, bss_pieces, bss_outside)]
        overlays[unit_name] = dict(
            unit=unit_name, path=f'game/extracted-iso/{file_name}', sha256=image.sha256,
            section=dict(name=unit_name, index=section['index'], va=hex32(section['va']), size=section['size'],
                         file_offset=section['offset'], flags=section['flags'], align=section['align'],
                         sha256=hashlib.sha256(body).hexdigest()),
            counts=dict(tus=len(records), functions=len(unit.vas), accepted_functions=0,
                        forced=dict(stats)),
            layout=dict(text_start=hex32(section['va']), text_end=hex32(image.text_end),
                        data_half_start=hex32(image.text_end), section_end=hex32(section['va'] + section['size']),
                        zero_tail_start=hex32(zero_from - section['offset'] + section['va']),
                        families=families,
                        family_boundary_windows=[], link_order_monotonic=monotonic, unowned_windows=[], unsymbolised_text=[]),
            tus=records, global_data_symbols=[], source_file_strings=sorted({f for r in records for f in r['source_file_strings']}),
            cross_tu_calls=[], link=dict(vma=hex32(section['va']), size=section['size'], end=hex32(section['va'] + section['size']),
                                         file_offset=section['offset']), provisional_comparison={}, anomalies=[])
        summary[unit_name] = dict(tus=len(records), functions=len(unit.vas), text_end=hex32(image.text_end),
                                  text_bytes=image.text_end - section['va'], stats=dict(stats))
    write_json(root / '.work/tu-migration/map-ovl/tu-map-ovl.json', dict(
        schema='x2-tu-map-ovl/1', task='TU map of the X2 overlays, inferred without symbols',
        identity=dict(revision='slus-20892-1f978e64c837'), analysis_only=True,
        generator='tools/tu/map_infer.py generate', inputs=dict(model=MODEL_FILE), summary=summary,
        linking={}, validation_point='Inferred; see reports/x2-tu-map.md', overlays=overlays, iop={}))
    return summary


def auc(values, labels):
    pairs = sorted(zip(values, labels), key=lambda pair: pair[0])
    positive = sum(labels)
    negative = len(labels) - positive
    if not positive or not negative:
        return None
    total = 0.0
    position = 0
    while position < len(pairs):
        last = position
        while last < len(pairs) and pairs[last][0] == pairs[position][0]:
            last += 1
        total += (position + last + 1) / 2 * sum(pair[1] for pair in pairs[position:last])
        position = last
    return round((total - positive * (positive + 1) / 2) / (positive * negative), 3)


def homolog_agreement(root=ROOT):
    """Evidence the model never saw, on X2 itself: X1 files of neighbouring homologs and __FILE__."""
    model_file = json.loads((root / MODEL_FILE).read_text())
    out = {}
    units = [('main', 'SLUS_208.92', 'annotations/slus_208.92_annotations.csv')] + list(OVERLAYS)
    values, labels = [], []
    disagreements = []
    file_pairs = collections.Counter()
    for unit_name, file_name, annotations in units:
        image = Image(root / 'game/extracted-iso' / file_name, unit_name)
        if image.overlay:
            image.text_end = find_text_end(image)
        unit = Unit(image)
        _, homologs = load_annotations(root / annotations)
        odds = score(model_file['model'], unit.features())
        ordered = sorted(unit.index[va] for va in homologs if va in unit.index)
        files = unit.source_files()
        for a, b in zip(ordered, ordered[1:]):
            first, second = homologs[unit.vas[a]][1], homologs[unit.vas[b]][1]
            if first == second and b - a <= HOMOLOG_SPAN:
                for gap in range(a, b):
                    values.append(odds[gap])
                    labels.append(0)
            elif first != second and b == a + 1:
                values.append(odds[a])
                labels.append(1)
        by_x1 = collections.defaultdict(set)
        for i in ordered:
            if i in files and len(files[i]) == 1:
                by_x1[homologs[unit.vas[i]][1]].add(next(iter(files[i])))
        for x1_file, names in by_x1.items():
            file_pairs['x1_file_with_one_x2_file' if len(names) == 1 else 'x1_file_with_several_x2_files'] += 1
            if len(names) > 1:
                disagreements.append(dict(unit=unit_name, x1_file=x1_file,
                                          x2_files=sorted(name.decode('latin1') for name in names)))
    out['model_auc_on_homolog_edges'] = auc(values, labels)
    out['homolog_gaps'] = len(labels)
    out['homolog_cut_gaps'] = sum(labels)
    out['x1_vs_file_path'] = dict(file_pairs)
    out['x1_files_split_in_x2'] = disagreements
    return out


def load_maps(root=ROOT):
    main_map = json.loads((root / '.work/tu-migration/map-main/tu-map-main.json').read_text())
    ovl_map = json.loads((root / '.work/tu-migration/map-ovl/tu-map-ovl.json').read_text())
    return main_map, ovl_map


def check_maps(root=ROOT):
    """Fail-closed integrity: full coverage, every function once, offsets that hold the bytes."""
    problems = []
    main_map, ovl_map = load_maps(root)
    image = Image(root / main_map['identity']['binary'], 'main')
    if image.sha256 != main_map['identity']['binary_sha256']:
        problems.append('main: binary sha256 differs')
    position = image.text_start
    seen = set()
    for tu in main_map['tus']:
        start, end = int(tu['start'], 16), int(tu['end'], 16)
        if start != position:
            problems.append(f'{tu["id"]}: starts at {start:#x}, previous ended at {position:#x}')
        position = end
        if tu['function_count'] != len(tu['functions']) or not tu['functions']:
            problems.append(f'{tu["id"]}: function count')
        if tu['in_scope'] != (tu['classification']['cls'] == 'game'):
            problems.append(f'{tu["id"]}: scope and class disagree')
        if tu['file_offset_start'] != start + image.base:
            problems.append(f'{tu["id"]}: file offset')
        for function in tu['functions']:
            va = int(function['va'], 16)
            if va in seen or not start <= va < end or va + function['size'] > end:
                problems.append(f'{tu["id"]}: function {function["name"]} outside its TU or repeated')
            seen.add(va)
            if function['file_offset'] != va + image.base:
                problems.append(f'{tu["id"]}: {function["name"]} file offset')
    if position != image.text_end:
        problems.append(f'main: TUs end at {position:#x}, .text ends at {image.text_end:#x}')
    if main_map['validation']['library_zone_calls_into_game'] != [['0x00120008', '0x001947b8']]:
        problems.append(f'main: library zones call game code: {main_map["validation"]["library_zone_calls_into_game"][:6]}')
    if not main_map['appendix_ov02_section']['identical_to_ov02_ovl']:
        problems.append('main: embedded ov02 differs from OV02.OVL')
    for unit, record in ovl_map['overlays'].items():
        image = Image(root / record['path'], unit)
        if image.sha256 != record['sha256']:
            problems.append(f'{unit}: binary sha256 differs')
        position = int(record['layout']['text_start'], 16)
        names = set()
        for tu in record['tus']:
            start, end = int(tu['text']['start'], 16), int(tu['text']['end'], 16)
            if start != position:
                problems.append(f'{tu["id"]}: starts at {start:#x}, previous ended at {position:#x}')
            position = end
            body = image.data[tu['text']['file_offset']:tu['text']['file_offset_end']]
            if hashlib.sha256(body).hexdigest() != tu['text']['sha256']:
                problems.append(f'{tu["id"]}: text hash')
            for function in tu['functions']:
                if function['name'] in names:
                    problems.append(f'{tu["id"]}: repeated name {function["name"]}')
                names.add(function['name'])
        if position != int(record['layout']['text_end'], 16):
            problems.append(f'{unit}: TUs end at {position:#x}')
        # The families tile the data half, and a .rodata family's TU pieces tile it in link order.
        families = record['layout']['families']
        cursor = int(record['layout']['text_end'], 16)
        for family in families:
            if int(family['start'], 16) != cursor:
                problems.append(f'{unit}: family {family["family"]} starts at {family["start"]}, expected {cursor:#x}')
            cursor = int(family['end'], 16)
            if family['family'] == 'rodata':
                at = int(family['start'], 16)
                for piece in family['owners']:
                    if int(piece['start'], 16) != at:
                        problems.append(f'{unit}: .rodata piece of {piece["id"]} starts at {piece["start"]}, expected {at:#x}')
                    at = int(piece['end'], 16)
                if at != cursor:
                    problems.append(f'{unit}: .rodata pieces end at {at:#x}, the family at {cursor:#x}')
                if not record['layout']['link_order_monotonic']:
                    problems.append(f'{unit}: .rodata pieces are not in TU order')
        if cursor != int(record['layout']['section_end'], 16):
            problems.append(f'{unit}: families end at {cursor:#x}')
    return problems


def write_report(root=ROOT):
    model_file = json.loads((root / MODEL_FILE).read_text())
    main_map, ovl_map = load_maps(root)
    agreement = homolog_agreement(root)
    game = [tu for tu in main_map['tus'] if tu['in_scope']]
    other = [tu for tu in main_map['tus'] if not tu['in_scope']]
    all_game = [(tu['boundary_confidence'], tu['function_count'], tu['size'], tu['suggested_name']['confidence']) for tu in game]
    for record in ovl_map['overlays'].values():
        all_game += [(tu['boundary_confidence'], tu['function_count'], tu['text']['size'], tu['suggested_name_confidence'])
                     for tu in record['tus']]
    confidence = collections.Counter(row[0] for row in all_game)
    naming = collections.Counter(row[3] for row in all_game)
    counts = sorted(row[1] for row in all_game)
    sizes = sorted(row[2] for row in all_game)
    functions = sum(counts)
    file_hits = model_file['forced_precision'].get('file_path', {}).get('hits', 0)
    ov12 = model_file['folds']['ov12']['exact']
    ov12_hits = round(ov12['precision'] * ov12['cuts'])
    x2_file_cuts = main_map['summary']['stats'].get('cuts_file_path', 0)
    x2_file_cuts += sum(record['counts']['forced'].get('cuts_file_path', 0) for record in ovl_map['overlays'].values())
    known = {'main': {f['va'] for tu in main_map['tus'] for f in tu['functions']}}
    for unit, record in ovl_map['overlays'].items():
        known[unit] = {f['va'] for tu in record['tus'] for f in tu['functions']}
    annotated = missing = 0
    for unit, annotations in [('main', 'annotations/slus_208.92_annotations.csv')] + [(u, a) for u, _, a in OVERLAYS]:
        names, _ = load_annotations(root / annotations)
        annotated += len(names)
        missing += sum(1 for va in names if hex32(va) not in known[unit])
    main_fold = model_file['folds']['main']
    held = main_fold['forced_by_kind']

    def holding(kind):
        row = held.get(kind, dict(intervals=0, holding=0))
        return f'{row["holding"]} of {row["intervals"]}'
    lines = [
        '# X2 TU map', '',
        f'Generated by `tools/tu/map_infer.py report`. Do not edit by hand.', '',
        'X2 keeps no symbols, so this map is inferred, and many of its boundaries are wrong.',
        'It is a work plan to start matching with. It is not a recovery of the original objects,',
        'and every boundary carries its evidence so that it can be corrected later.', '',
        '## Scope', '',
        'Only game code is in scope, by user decision of 2026-11-05: Sony SDK, public libraries and',
        'middleware (CRI, Sofdec, KORG) are excluded. VU microcode counts as game code.', '',
        '| Zone | Range | Class | In scope | Functions | Bytes |', '| --- | --- | --- | --- | --- | --- |']
    for tu in other:
        lines.append(f'| {tu["suggested_name"]["name"]} | `{tu["start"]}-{tu["end"]}` | {tu["classification"]["cls"]} | no | {tu["function_count"]} | {tu["size"]:,} |')
    summary = main_map['summary']
    lines.append(f'| game, main | `{game[0]["start"]}-{game[-1]["end"]}` | game | yes | {summary["game_functions"]} | {summary["game_bytes"]:,} |')
    for unit, record in ovl_map['overlays'].items():
        size = int(record['layout']['text_end'], 16) - int(record['layout']['text_start'], 16)
        lines.append(f'| game, {unit} | `{record["layout"]["text_start"]}-{record["layout"]["text_end"]}` | game | yes | {record["counts"]["functions"]} | {size:,} |')
    vu = main_map['non_text_objects']
    lines += ['',
              f'VU microcode, also in scope and credited apart as `exact_vu_microcode`: `.vutext` ({vu["sections"][".vutext"]["size"]:,} bytes),',
              f'`.vudata` ({vu["sections"][".vudata"]["size"]:,} bytes) and {sum(len(o["dvp_overlays"]) for o in vu["vu_microcode_objects"])} `.DVP.overlay` sections ({vu["overlay_bytes"]:,} bytes, '
              f'{len(vu["vu_microcode_objects"])} assembler objects by',
              'their section names). They are listed under `non_text_objects`, not split into programs.', '',
              'The call graph checks the zones: the only call from a library zone into game code is crt0',
              'to `main` (`0x120008` to `0x1947b8`). Inside the game zone, `0x1a4648` binds the KORG driver,',
              'but it is game code that uses the client: the client itself is the out-of-scope block above.', '',
              '## Result', '',
              f'{len(all_game)} game TUs and {functions:,} functions. Functions per TU: median {counts[len(counts) // 2]}, '
              f'largest {counts[-1]}. Bytes per TU: median {sizes[len(sizes) // 2]:,}.', '',
              '| Boundary confidence | TUs |', '| --- | --- |']
    lines += [f'| {key} | {value} |' for key, value in sorted(confidence.items())]
    lines += ['', '| Name confidence | TUs |', '| --- | --- |']
    lines += [f'| {key} | {value} |' for key, value in sorted(naming.items())]
    lines += ['', '## How far to trust it', '',
              'Every number below comes from Xenosaga I, scored on a unit the model did not train on.',
              'X2 cannot be scored this way, because it has no truth to compare with.', '',
              '| Held-out X1 unit | True boundaries | Cuts | Exact precision | Exact recall | Exact F1 | F1 within 2 functions |',
              '| --- | --- | --- | --- | --- | --- | --- |']
    for unit, row in model_file['folds'].items():
        lines.append(f'| {unit} | {row["truth"]} | {row["exact"]["cuts"]} | {row["exact"]["precision"]} | {row["exact"]["recall"]} | {row["exact"]["f1"]} | {row["within_2"]["f1"]} |')
    pooled = model_file['pooled']
    lines += [f'| all | {pooled["truth"]} | {pooled["cuts"]} | {pooled["precision"]} | {pooled["recall"]} | {pooled["f1"]} | |', '',
              f'Assert strings explain most of the gap between ov12 and the rest: {file_hits} of its {ov12_hits} correct cuts',
              f'come from `__FILE__` names. They choose only {x2_file_cuts} of the {len(all_game)} X2 boundaries, so expect results',
              'closer to X1 main than to ov12.', '',
              'Precision of a cut by what chose it, held out on X1:', '',
              '| Chosen by | Cuts | Correct gap | Precision |', '| --- | --- | --- | --- |']
    for kind, row in sorted(model_file['forced_precision'].items()):
        lines.append(f'| {kind} | {row["cuts"]} | {row.get("hits", 0)} | {row["precision"]} |')
    model_cuts = sum(row['cuts'] for row in model_file['precision_by_log_odds'].values())
    model_hits = sum(row['hits'] for row in model_file['precision_by_log_odds'].values())
    lines += [f'| model score only | {model_cuts} | {model_hits} | {round(model_hits / model_cuts, 3)} |', '',
              '`x1_homolog` and `x1_c_o0` cuts have no row, because X1 cannot score its own homologs. For',
              '`dup_string`, `lit4_step` and `rodata_pad` the interval holds an edge far more often than the chosen',
              'gap is that edge, so those cuts are low confidence unless the gap precision reaches 0.5. A model-only cut is',
              'right about one time in four, against a base rate of one gap in twenty.', '',
              'Measured on X2 with evidence the model never saw:', '',
              f'- Model AUC on the edges that X1 homologs mark: {agreement["model_auc_on_homolog_edges"]}, over '
              f'{agreement["homolog_gaps"]} gaps. Only {agreement["homolog_cut_gaps"]} of them are cuts, too few to lean on.',
              f'- Annotated function starts missing from the table: {missing} of {annotated}. An earlier estimator',
              '  helped produce those annotations, so this is a consistency check rather than an independent one.',
              ('- Functions with both an X1 homolog and a `__FILE__` name: none, so those two kinds of evidence'
               if not agreement['x1_vs_file_path'] else f'- X1 homolog files against `__FILE__` names: {agreement["x1_vs_file_path"]}.'),
              ('  cannot be checked against each other.' if not agreement['x1_vs_file_path'] else '')]
    estimator = model_file['function_estimator']
    lines += ['', '## Function table', '',
              '| X1 unit | True functions | Found | Recall | Precision | True text end | Found text end |',
              '| --- | --- | --- | --- | --- | --- | --- |']
    for unit, row in estimator.items():
        lines.append(f'| {unit} | {row["truth"]} | {row["found"]} | {row["recall"]} | {row["precision"]} | {row.get("text_end_true", "-")} | {row.get("text_end_found", "-")} |')
    lines += ['', '## Why boundaries need so much inference', '',
              'Measured on X1, these looked promising and carry no signal:', '',
              '- Function alignment and padding: gap sizes and 16-byte starts are the same at object edges and inside objects.',
              '- Duplicate float literals: each `li.s` gets its own `.lit4` word, so a repeated value says nothing about objects.',
              '- `-G` mode: the code only shows evidence for `-G8`, and neighbouring functions never disagree.',
              '- One datum reached by both `$gp` and `lui`: 2 addresses on X1 main, 7 on X2, too few to use.', '',
              'What works on X1 main, none of it enough alone:', '',
              '- A shared string forbids a cut: 0 true boundaries blocked on any X1 unit.',
              f'- A local datum on the left above one on the right forbids a cut, within {INVERSION_WINDOW} functions and '
              f'{INVERSION_DISTANCE} bytes: {main_fold["blocked_true"]} true boundaries',
              f'  blocked, with {main_fold["blocked_inside"]} inner gaps forbidden by this rule and the string rule together.',
              f'- Two copies of one string require a cut, because cc1 pools strings per object and ld never merges them: {holding("dup_string")} intervals held a boundary.',
              f'- `.lit4` not continuing requires a cut: {holding("lit4_step")}.',
              f'- A string followed by 8 zero bytes or more, up to a 16-aligned item in `.rodata`, requires a cut: {holding("rodata_pad")}.',
              '- Two different `__FILE__` names require a cut, and call, data and pool-step cohesion rank the rest.', '',
              'These rules are precise but rare. With all of them, exact F1 went from 0.337 to '
              f'{main_fold["exact"]["f1"]} on X1 main and from 0.416 to {pooled["f1"]}',
              'over all units, so static inference looks close to its ceiling. They do not fire on overlays, whose data',
              'sections are not separated.', '',
              'splat does not supply boundaries: it splits where its config says, and here that config is',
              'generated from this map. Its `find_file_boundaries` option only prints hints, and spimdisasm 1.42.4',
              'disables the rule for `EEGCC` (no `sectionAlign_text`), so it gives 0 hints on X1 and X2. Forcing',
              'the alignment on X1 main gives 1,174 hints at 8 bytes and 584 at 16, with 4.9% and 5.3% precision:',
              'the same as the 5% of gaps that are boundaries. The rule looks for padding, which carries no signal here.', '',
              'X2 orders data unlike text around the `-O0` zone: text runs code before the zone, the zone, code after it,',
              'and the data sections run code after it, code before it, the zone. Inside a run, a jump in `.rodata` between',
              f'runs that touch in `.text` suggests an object edge. On X1 main, {holding("rodata_order")} such intervals in game code held a',
              'true boundary, so these cuts are medium confidence.', '',
              '## The `-O0` zone', '',
              f'`{O0_HOMOLOGS_FILE}` lists bodies equal to X1\'s C compiled at `-O0` (reports/x2-o0-provenance.md).',
              f'Neighbours from one X1 file, in X1 order and within {O0_HOMOLOG_SPAN} functions, hold no cut. Neighbours from two files need a',
              f'cut only when both bodies have {O0_CUT_BYTES} bytes or more, because at 64-68 bytes X1 functions still collide. X1 cannot score this rule.', '',
              'Padding says nothing here: the zone has none in 478 gaps, but an `-O0` object has `.text` aligned to 4 and no',
              'tail padding, so this does not show that the zone is one object.', '',
              '## What other PS2 projects do', '',
              'Read on 2026-11-05, from each project\'s documentation. None recovers exact boundaries from a stripped binary.', '',
              '| Source of boundaries | Projects |', '| --- | --- |',
              '| Symbols of a prototype | Sly 1, PaRappa 2, Silent Hill 2 |',
              '| `.mdebug` in the retail file | Crash: Wrath of Cortex (267 units) |',
              '| Estimated from signals | Final Fantasy X (375 TUs: 15 high, 97 medium, 263 low), Ratchet & Clank 3 (65 files) |',
              '| C++ classes or RTTI | God Hand, GT4; X2 has no such data |',
              '| One function per file, no boundaries | THUG, Shadow of the Colossus, GT4 |', '',
              'Only another build with symbols would give real boundaries. Disc 2 boots a byte-identical copy of `SLUS_208.92`.', '',
              '## Xenosaga Freaks', '',
              'Xenosaga Freaks (Japan, `SLPS_253.53`, read 2026-11-05) gives no boundaries. No EE ELF on the disc has a `.symtab`, and',
              'every `.mdebug.eabi64` is empty. `EXP/EXP.ELF` is another build of this engine, with 22,536 fewer bytes of `.text`.',
              'Comparing it with `SLUS_208.92` was tried three ways:', '',
              '- Link order: 2,529 of 2,530 uniquely equal functions run in one order, so no block of code moved.',
              '- Changed code: of 583 gaps beside a changed or missing function, 7 (1.2%) hold a `__FILE__` or `-O` cut, against 44 of 3,773',
              '  (1.2%) over all gaps. A change does not mark an object edge.',
              '- Data: only 3 steps between the builds are pure padding, and an aligned item inside one object could explain them.', '',
              'The other executables (`ADV`, `DTB`, `MINI`, `SLPS_253.53`) share at most 7 game functions with X2. Nothing from this disc',
              'feeds the map. The disc holds no ground truth, so none of this was scored.', '',
              '## What a wrong boundary costs', '',
              'The acceptance gate compares the linked image, and data stays in scaffolding until someone claims',
              'it, so most functions can match under a wrong map. Not all of them: a C function defined earlier in the same',
              'object changes how cc1 treats calls to it. On 22 X1 C files, removing the body of an earlier callee changed the',
              'caller in 14 of 175 edges (14 of 144 in large files, 0 of 31 in small ones), usually one line, with no change',
              'in 3,216 comparisons of functions that do not call it. This was a one-off measurement. A callee that is only a',
              'stub and the opposite case, a callee wrongly placed in the same file, were not tested, and nothing has run on X2.', '',
              'So a one-line residue next to a call to an earlier function can be evidence about the boundary, and a wrong',
              'boundary can cost a match as well as readability.', '',
              '## Not done here', '',
              '- Compiler evidence from matching is not recorded yet, so nothing corrects the map as functions match.',
              '- Overlay data: ov01 (0xAB8CA0-0xABB870) and ov02 (0xA83178-0xA83C48) have a per-TU `.rodata` family (OVERLAY_RODATA) and per-TU `.data` and `.bss` families cut by link order (OVERLAY_DATA_SECTIONS).',
              '- VU programs are listed, not split, and the VU tools have not been tried on X2.',
              '- `section_order` covers code-referenced data only. `binding` is `unknown` for every function.', '']
    (root / 'reports/x2-tu-map.md').write_text('\n'.join(lines))
    return agreement


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=1, sort_keys=False) + '\n')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('functions', help='score the function estimator on X1 symbols')
    sub.add_parser('calibrate', help='score the boundary model on X1, write ' + MODEL_FILE)
    sub.add_parser('generate', help='write the X2 TU maps')
    sub.add_parser('check', help='verify the maps against the originals and each other')
    sub.add_parser('report', help='write reports/x2-tu-map.md')
    args = parser.parse_args(argv)
    if args.command == 'functions':
        for unit, row in score_functions().items():
            print(unit, row)
    elif args.command == 'calibrate':
        result = calibrate()
        write_json(ROOT / MODEL_FILE, result)
        for unit, row in result['folds'].items():
            print(unit, json.dumps({k: v for k, v in row.items()}))
        print('pooled', result['pooled'])
        print('by log-odds', result['precision_by_log_odds'])
        print('forced', result['forced_precision'])
        print('functions', result['function_estimator'])
    elif args.command == 'generate':
        summary = generate()
        print(json.dumps(summary, indent=1))
    elif args.command == 'check':
        problems = check_maps()
        for problem in problems:
            print('FAIL', problem)
        print('check', 'failed: ' + str(len(problems)) if problems else 'ok')
        return 1 if problems else 0
    else:
        print(json.dumps(write_report(), indent=1))
    return 0


if __name__ == '__main__':
    sys.exit(main())
