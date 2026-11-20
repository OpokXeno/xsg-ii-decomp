#!/usr/bin/env python3
"""Generate the tracked build inputs of the X2 scaffold from the inferred TU maps.

    python3 -B tools/tu/x2_scaffold.py          # write config/split, symbols, objects
    python3 -B tools/tu/x2_scaffold.py --check  # say which inputs differ, write nothing

Inputs are the maps `tools/tu/map_infer.py` writes under .work/tu-migration and the
originals in game/extracted-iso. Everything here is scaffolding: every game TU builds
from INCLUDE_ASM and earns no credit, and the whole-file gate alone says whether the
result is right (docs/x2-bootstrap.md, phase 4).

X2 has no symbol table, so three things are estimates, and the output says so:

- Function starts, sizes and names (config/symbols/estimated-functions.json,
  `provenance: estimated`). tools/tu/elfinfo.py serves that table as `Elf.symbols`
  for the three originals. Every binding is assumed GLOBAL.
- Data pieces of main. Each TU's piece starts at its first anchored reference and runs
  to the next TU's, so bytes between two objects go to the earlier one. Byte identity
  does not depend on these cuts.
- The overlay data half, one unsplit `.data` piece plus a `.bss` piece that starts at
  the zero tail. Where the map gives the overlay a `.rodata` family (map_infer.py
  OVERLAY_RODATA, ov01), the `.data` piece ends at it and each TU's `.rodata` piece is
  its own `rodata` subsegment, cut as main's data pieces.

The compile contract of a TU comes from reports/x2-compiler-calibration.md: `-O0` inside
main's unoptimized zone, otherwise `-O2`; `-G8` when the code has a gp-relative reference,
otherwise `-G0`, which cannot add one.
"""
import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tu"))
from elfinfo import Elf  # noqa: E402

MAIN_MAP = ROOT / ".work/tu-migration/map-main/tu-map-main.json"
OVL_MAP = ROOT / ".work/tu-migration/map-ovl/tu-map-ovl.json"
REVIEWED_NAMES = ROOT / "config/symbols/function-names.json"
REVIEWED_DATA = ROOT / "config/symbols/data-names.json"
GAME = ROOT / "game/extracted-iso"
OVERLAYS = ("ov01", "ov02")
FILES = dict(main="SLUS_208.92", ov01="OV01.OVL", ov02="OV02.OVL")
GP_REG = 28
# I-type opcodes with a base register or a source: addiu, loads, stores, FP and quadword.
GP_OPS = {0x09, 0x1a, 0x1b, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
          0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x31, 0x36, 0x37, 0x39, 0x3e, 0x3f}
DATA_SECTIONS = (".data", ".rodata", ".lit4", ".sdata", ".sbss", ".bss")
SPLAT_TYPE = {".data": "data", ".rodata": "rodata", ".lit4": "lit4", ".sdata": "sdata",
              ".sbss": "sbss", ".bss": "bss"}

CC1_SHA = "25944d5c023a48316d719ff10b13a366afc31c3209662f28e440a90eb43d3b17"
AS_SWAP_SHA = "448143a3f1f7ecd4fffb79ea3e641afa36863895366873f93462028535f95494"
AS_PLAIN_SHA = "8c55ab9beeb4834c6cc0e841379b81930e329496f85893e8607d5b772c2b51ff"
COMPILER_ID = "ee-gcc2_96-realconv-lp7"
COMPILER_DIR = "tool:ee-gcc2.96-realconv-lp7"
AS_SWAP_NAME = "LA29-vsqrt-swap-g (corrected ee-as with the per-object flag -g)"
AS_PLAIN_NAME = "LA29-vsqrt-nolabel-laimm (runtime and hand-written objects)"
# Soft-float entry points of libgcc's fp-bit.c (US_SOFTWARE_GOFAST spellings, see
# config/library-references.json "gnu-fp-bit-gofast"). The pinned cc1 calls them by these names;
# each address is witnessed by a matched call site in main (func_00244938: int->double, multiply,
# divide, double->float in that order). Main's table keeps the func_ names; C reaches them here.
# The 64-bit integer division helpers of libgcc2.c follow the same rule (xglFontFlushCore:
# long long % 10, then / 10). dpcmp: the double compare of gifStateEffectFrame.
SOFT_FLOAT_LIBCALLS = {0x002A7560: "litodp", 0x002A70E8: "dpmul", 0x002A7398: "dpdiv",
                       0x002A76E8: "dptofp", 0x002A6D88: "fptodp", 0x002A4F60: "__divdi3",
                       0x002A55F8: "__moddi3", 0x002A7510: "dpcmp", 0x002A7080: "dpsub",
                       # upstream PR #23 (textTagDrawGlyph casts long long to float) and its siblings
                       0x002A6818: "__floatdisf", 0x002A7618: "dptoli", 0x002A6BB8: "__fixsfdi",
                       0x002A6A18: "__fixdfdi"}
# Addresses main's C reaches that no table offers: entries inside an OV01 function and scaffold
# data without a label at that address. Hand edits of the output were lost at a regeneration
# (2026-11-13), so they live here. Renamed labels must be renamed here too.
EXTRA_PROVIDES = (
    ("testCfEventToolRun", 0x00A89428, "test overlay 98 entry, mid-function in ov01: testMenuCallbacks[0] (CFEventTool)"),
    ("testWindowGetUserData", 0x00A920EC, "test overlay 98 entry, mid-function in ov01: the heap window's user data"),
    ("testOverlayMenuOpen", 0x00A82EEC, "test overlay 98 entry called by src/main/test_128.c (not a function of main/ov01/ov02)"),
    ("testWindowDrawLine", 0x00A90DF8, "test overlay 98 entry called by src/main/test_128.c (not a function of main/ov01/ov02)"),
    ("testWindowShow", 0x00A91668, "test overlay 98 entry called by src/main/test_128.c (not a function of main/ov01/ov02)"),
    ("testWindowFindByName", 0x00A917D4, "test overlay 98 entry called by src/main/test_128.c (not a function of main/ov01/ov02)"),
    ("testWindowGetInfo", 0x00A91AF4, "test overlay 98 entry called by src/main/test_128.c (not a function of main/ov01/ov02)"),
    ("testWindowClearRows", 0x00A91E04, "test overlay 98 entry called by src/main/test_128.c (not a function of main/ov01/ov02)"),
    ("testWindowSetColumnTable", 0x00A9229C, "test overlay 98 entry called by src/main/test_128.c (not a function of main/ov01/ov02)"),
    ("testWindowAddRow", 0x00A923D8, "test overlay 98 entry called by src/main/test_128.c (not a function of main/ov01/ov02)"),
    ("testWindowSetRefreshCallback", 0x00A9262C, "test overlay 98 entry called by src/main/test_128.c (not a function of main/ov01/ov02)"),
    ("testWindowCreate", 0x00A92704, "test overlay 98 entry called by src/main/test_128.c (not a function of main/ov01/ov02)"),
    ("testWindowSetText", 0x00AB847C, "test overlay 98 entry called by src/main/test_128.c (not a function of main/ov01/ov02)"),
    ("D_0069DA38", 0x0069DA38, "tu126 .sdata: 8-byte SJIS mark entry; scaffold label split at +1 (D_0069DA39)"),
    ("D_0069DA40", 0x0069DA40, "tu126 .sdata: 8-byte SJIS mark entry; scaffold label split at +1 (D_0069DA41)"),
    ("D_0069DA48", 0x0069DA48, "tu126 .sdata: 8-byte SJIS mark entry; scaffold label split at +1 (D_0069DA49)"),
    ("D_006AC0C0", 0x006AC0C0, "tu126 saveLabelBuild data; no scaffold label at this address"),
    ("D_0069D101", 0x0069D101, "tu060 .sdata: second byte of the 8-byte D_0069D100 (func_001EC8E8 name/owner column flag); scaffold label split at +1"),
)
# MAIN data an overlay's C reads by name (the overlay asm reaches it with an absolute lui/offset pair that
# carries no relocation, so no scaffold table offers the name). Same names as main's C.
OVERLAY_MAIN_DATA = {
    "ov01": (("D_0069DC7C", 0x0069DC7C, "main .sdata byte (tu110 SaveData.word2E6C source); read by ov01 tu023 battleWorkTablesReset"),
        ("dataPadRawWords", 0x0063EDB0, "main .data pad state halfwords (xgl_dma.c / tu100.c); read by ov01 tu022 dataPadRead"),
        ("textOptionFlags", 0x0063EDAA, "main .data text option halfword; read by ov01 tu072/tu073 debug value pages"),
        ("D_0063EDAC", 0x0063EDAC, "main .data halfword after textOptionFlags; read by ov01 tu073 debugValLR"),
        ("ClearEnv", 0x006457C0, "main .data clear environment colour; read by ov01 tu074 ov01SceneRun"),
        ("gameWork", 0x002B5D90, "main .bss game work record (tu048.h GameWork); read by ov01 code"),
        ("PadData", 0x0063ED80, "main .data pad record; read by ov01 code"),
        ("MMathUnitVector", 0x0068C660, "main .rodata unit vector (m_math_191.c); read by ov01 MEf path/Hermite code"),
        ("MMathUnitMatrix", 0x0068C670, "main .rodata unit matrix (m_math_191.c); read by ov01 tu077 MSeGetActorMatrix"),
        ("seDefaultVolume", 0x00644F1E, "main .data default SE volume byte (tu071.c seVol); read by ov01 tu071 seObjectVoiceStart"),
        ("D_0061B2D0", 0x0061B2D0, "main .data matrix at +0x10 of scaffold D_0061B2C0 (no C owner yet); read by ov01 tu084/tu089/tu090"),),
    "ov02": (("D_0069E094", 0x0069E094, "main signed char (src/main/xgl_render.c); written by ov02 tu001 Title"),),
}
CONTRACT_BASIS = ("X1's last patched cc1 and corrected assembler with -g; "
                  "see reports/x2-compiler-calibration.md")


def hx(value):
    return int(value, 16)


def h8(value):
    return f"0x{value:08x}"


def up(value):
    return f"0x{value:X}"


def align_up(value, alignment):
    return (value + alignment - 1) & ~(alignment - 1)


def json_text(data):
    return json.dumps(data, indent=1) + "\n"


def tu_file_name(tu, used):
    """A TU's file stem: its evidence-backed name, otherwise its neutral id."""
    ordinal = f"tu{tu['ordinal']:03d}"
    if tu.get("file_name"):     # an overlay TU named by its X1 file (tools/tu/map_infer.py X1_FILE_NAMES)
        return tu["file_name"]
    suggestion = tu.get("suggested_name") or {}
    if not isinstance(suggestion, dict) or suggestion.get("confidence") not in ("high", "medium"):
        return ordinal
    stem = re.sub(r"\W", "_", re.sub(r"\.(c|s)$", "", suggestion["name"]))
    if not stem or stem[0].isdigit():
        return ordinal
    return f"{stem}_{tu['ordinal']:03d}" if used.get(stem, 0) > 1 else stem


def stem_counts(tus):
    used = {}
    for tu in tus:
        suggestion = tu.get("suggested_name") or {}
        if isinstance(suggestion, dict) and suggestion.get("confidence") in ("high", "medium"):
            stem = re.sub(r"\W", "_", re.sub(r"\.(c|s)$", "", suggestion["name"]))
            used[stem] = used.get(stem, 0) + 1
    return used


class Image:
    def __init__(self, unit):
        self.unit = unit
        self.file = FILES[unit]
        self.data = (GAME / self.file).read_bytes()
        self.elf = Elf(self.data)
        self.sections = {s.name: s for s in self.elf.sections}
        self.sha256 = hashlib.sha256(self.data).hexdigest()
        self.sha1 = hashlib.sha1(self.data).hexdigest()

    def gp_references(self, section, start, end):
        base = section.offset - section.addr
        count = 0
        for va in range(start & ~3, end & ~3, 4):
            word = struct.unpack_from("<I", self.data, va + base)[0]
            if (word >> 21) & 31 == GP_REG and word >> 26 in GP_OPS:
                count += 1
        return count


def unique_names(functions):
    """Function names of one unit: annotation names only where they are unique."""
    counts = {}
    for f in functions:
        counts[f["name"]] = counts.get(f["name"], 0) + 1
    out = {}
    for f in functions:
        va = hx(f["va"])
        out[va] = f["name"] if counts[f["name"]] == 1 else f"func_{va:08X}"
    return out


def compiler_record():
    return dict(id=COMPILER_ID, version="2.96-ee-001003-1", dir=COMPILER_DIR,
                sha256=dict(cc1=CC1_SHA), source="pinned release")


def main_contract(optimization, g_flag):
    return dict(assembler=AS_SWAP_NAME, assembler_path="tool:ee-as-la29-vsqrt-swap",
                assembler_sha256=AS_SWAP_SHA, cc1_sha256=CC1_SHA, cc_dir=COMPILER_DIR,
                compiler_profile=COMPILER_ID, flags=[optimization, g_flag], language="c",
                source_class="c", assembler_options=["-g"])


def asm_contract():
    return dict(assembler=AS_PLAIN_NAME, assembler_path="tool:ee-as-la29-vsqrt",
                assembler_sha256=AS_PLAIN_SHA, compiler_profile=None, flags=["-G0"],
                language="asm", source_class="hand-asm")


# TUs with no gp-relative instruction that are -G8 all the same, keyed by text start: their code loads the
# address of string literals of 8 bytes or less that sit in .sdata. GCC 2.96 puts such a literal in .sdata only
# under -G8 (mips_select_section) and still addresses it with lui/addiu, because only variables get the
# gp-relative flag (ENCODE_SECTION_INFO); a -G0 file emits nothing into .sdata (data pass, 2026-11-16).
SMALL_DATA_LITERAL_TUS = {
    0x0019A148: "xgl_menu: the three row formats of xglMenuDrawType1Sub (0x69DEF0, 0x69DEF8, 0x69DF00)",
    0x0022D1D8: "tu111: \"NULL\" (0x69D778) of smsg and msgDtl",
    # Data of 8 bytes or less in .sdata with code identical under -G8 (data pass 6, 2026-11-17).
    0x00251E30: "sef_print: \"RESET\" (0x69DAD8) and the shared words D_0069DAD0/svSceneFlags in .sdata",
    0x0027DCB0: "tu225: \"$prop1;\" (0x69DCB0) in .sdata",
    # The native names of a data table: .sdata literals of a -G8 file (data map review, 2026-11-17).
    0x001FC558: "tu079: \"math\" (0x69D3B8, mathInit) and the mathNativeTable names (0x69D398-0x69D3B0) in .sdata",
}


def g_evidence(gp_count, optimization, start=None):
    g_flag = "-G8" if gp_count or start in SMALL_DATA_LITERAL_TUS else "-G0"
    basis = (f"{gp_count} gp-relative instructions in the code" if gp_count else
             f"string literals in .sdata reached with lui/addiu: {SMALL_DATA_LITERAL_TUS[start]}"
             if start in SMALL_DATA_LITERAL_TUS else
             "no gp-relative instruction in the code; -G0 cannot add one")
    return g_flag, dict(compiler="2.96", compiler_confidence="medium", assembler_class="LA29",
                        assembler_confidence="medium", g_flag=g_flag, g_basis=basis,
                        g_confidence="high" if gp_count else "medium",
                        optimization=optimization,
                        source="reports/x2-compiler-calibration.md")


# ---------------------------------------------------------------- common YAML pieces

def yaml_options(unit, image, overlay):
    target = f"orig/{image.file}"
    lines = [f"name: xenosaga2 {unit} per-TU scaffold (generated by tools/tu/x2_scaffold.py)",
             f"sha1: {image.sha1}", "options:", "  platform: ps2", "  compiler: EEGCC",
             "  endianness: little", "  base_path: .", f"  target_path: {target}",
             f"  basename: {unit}"]
    if overlay:
        lines += ["  asm_path: asm", "  src_path: scaffold/src"]
    else:
        lines += ["  asm_path: asm/main", "  src_path: scaffold/src/main", "  hasm_in_src_path: True",
                  "  migrate_rodata_to_functions: False", "  pair_rodata_to_text: False"]
    lines += ["  auto_decompile_empty_functions: False", "  disassemble_all: True",
              "  build_path: build",
              f"  asset_path: {'assets' if overlay else 'assets/main'}",
              f"  ld_script_path: {unit}.ld", "  symbol_addrs_path:", "    - symbol_addrs.txt",
              "  undefined_funcs_auto_path: undefined_funcs_auto.txt",
              "  undefined_syms_auto_path: undefined_syms_auto.txt", "  o_as_suffix: True",
              "  create_asm_dependencies: False", "  ld_dependencies: False",
              "  find_file_boundaries: False", "  disasm_unknown: True",
              "  string_encoding: ASCII", "  data_string_encoding: ASCII",
              "  rodata_string_guesser_level: 2", "  data_string_guesser_level: 2",
              "  named_regs_for_c_funcs: False", "  mips_abi_gpr: numeric",
              "  mips_abi_float_regs: numeric", "  generate_asm_macros_files: True",
              "  asm_function_macro: glabel", "  asm_data_macro: dlabel",
              "  asm_end_label: endlabel", "  asm_emit_size_directive: True",
              "  use_legacy_include_asm: False", "  ld_fill_value: 0", "  section_order:",
              "    - .text", "    - .data", "    - .rodata", "    - .lit4", "    - .sdata",
              "    - .sbss", "    - .bss"]
    if overlay:
        lines.append("  migrate_rodata_to_functions: False")
    else:
        gp = image.elf.gp
        lines += [f"  gp_value: 0x{gp:08X}", f'  ld_gp_expression: "0x{gp:08X}"']
    return lines


# Data tables whose leading pointers splat's string guesser misreads as text: their first words
# point at .rodata strings whose address bytes (50 5C 68 00, "P\\h") look like a short string.
# A declared word type keeps them `.word LABEL`, so the scaffold item references its strings
# (data map review 2026-11-17: the CD file name tables of res_get_path).
TYPED_DATA_SYMBOLS = {
    "main": {va: (f"D_{va:08X}", "u32") for va in (0x002B9DE0, 0x002BA1C8, 0x002BA2A0, 0x002BA3E8, 0x002BA538)},
}


def data_symbol_lines(unit):
    """Data labels chosen for one unit's link (tools/tu/x1_port.py plan), after its functions.
    A typed label takes its reviewed name when config/symbols/data-names.json has one (one symbol per address)."""
    wanted = {}
    if REVIEWED_DATA.is_file():
        wanted = {int(va, 16): name for va, name in
                  (json.loads(REVIEWED_DATA.read_bytes()).get("units", {}).get(unit) or {}).items()}
    typed_rows = TYPED_DATA_SYMBOLS.get(unit) or {}
    typed = "".join(f"{wanted.get(va, name)} = 0x{va:08X}; // type:{kind}\n"
                    for va, (name, kind) in sorted(typed_rows.items()))
    return typed + "".join(f"{name} = 0x{va:08X};\n" for va, name in sorted(wanted.items()) if va not in typed_rows)


def overlay_main_data_lines(unit, taken):
    """Every reviewed MAIN data name as an absolute symbol of one overlay's link, so overlay C can
    name main data the way main's C does. Names the overlay already defines or lists win."""
    if not REVIEWED_DATA.is_file():
        return ""
    units = json.loads(REVIEWED_DATA.read_bytes()).get("units", {})
    taken = set(taken) | set((units.get(unit) or {}).values())
    rows = sorted((int(va, 16), name) for va, name in (units.get("main") or {}).items() if name not in taken)
    return "".join(f"{name} = 0x{va:08X}; /* main data (config/symbols/data-names.json) */\n" for va, name in rows)


def symbol_line(name, va, size=None):
    tail = "type:func" + (f" size:0x{size:X}" if size else "")
    return f"{name} = 0x{va:08X}; // {tail}\n"


# ---------------------------------------------------------------- overlays

def build_overlay(unit, image, record, main_names, out):
    section = image.sections[unit]
    layout = record["layout"]
    tus = record["tus"]
    text_end = hx(layout["text_end"])
    section_end = hx(layout["section_end"])
    bss_start = align_up(hx(layout["zero_tail_start"]), 8)
    if bss_start >= section_end:
        bss_start = section_end
    base = section.offset - section.addr
    used = stem_counts(tus)
    functions = [f for tu in tus for f in tu["functions"]]
    names = unique_names(functions)
    data_name = f"{unit}/unsplit"
    file_names = {}

    lines = yaml_options(unit, image, True)
    lines += ["segments:", "  - [0x0, bin, elf_header]", f"  - name: {unit}", "    type: code",
              f"    start: {up(section.offset)}", f"    vram: 0x{section.addr:08X}",
              f"    bss_size: {up(section_end - bss_start)}", "    align: 4", "    subalign: null",
              "    subsegments:"]
    entries, layout_text = [], {}
    for tu in tus:
        stem = tu_file_name(tu, used)
        file_names[tu["ordinal"]] = stem
        start, end = hx(tu["text"]["start"]), hx(tu["text"]["end"])
        lines.append(f"      - [{up(start + base)}, c, {unit}/{stem}]")
        layout_text[f"{unit}/{stem}"] = [h8(start), h8(end)]
        entries.append(dict(
            ordinal=tu["ordinal"], id=tu["id"], name=stem, text=[h8(start), h8(end)],
            functions=len(tu["functions"]), category="game", in_scope=True,
            toolchain_classification=dict(compiler="2.96", assembler="legacy-buggy", g_flag="-G0",
                                          id=f"{unit}#{tu['ordinal']}"),
            accepted_records=[], accepted_functions=[], kind="c",
            source=f"src/{unit}/{stem}.c", object=f"build/src/{unit}/{stem}.o",
            compiler=compiler_record(), flags=["-O2", "-G0"],
            assembler=dict(id=AS_SWAP_NAME, path="tool:ee-as-la29-vsqrt-swap",
                           sha256=AS_SWAP_SHA, argv="-EL -m5900 -mabi=eabi <G> -g -I .",
                           basis=CONTRACT_BASIS),
            contract_basis=dict(decision=CONTRACT_BASIS, g_flag_classification="-G0",
                                g_basis="no gp-relative instruction in the whole overlay"),
            include_asm_files=[f"asm/nonmatchings/{unit}/{stem}/{names[hx(f['va'])]}.s"
                               for f in tu["functions"]],
            accepted_asm_files=[], c_functions=[], accepted_asm_functions=[], local_renames={},
            adaptations=[], header_divergence=[]))
    # A `.rodata` family of the map (tools/tu/map_infer.py OVERLAY_RODATA): the data piece ends where it
    # starts, and each TU's piece is a splat `rodata` subsegment of that TU, as main's data families.
    rodata = next((f for f in layout.get("families", []) if f["family"] == "rodata"), None)
    data_end = hx(rodata["start"]) if rodata else bss_start
    rodata_rows = []
    if rodata:
        stem_of = {tu["ordinal"]: file_names[tu["ordinal"]] for tu in tus}
        rodata_rows = [[f"{unit}/{stem_of[p['ordinal']]}", h8(hx(p["start"])), h8(hx(p["end"]))]
                       for p in rodata["owners"]]
        if hx(rodata["end"]) != bss_start:
            raise SystemExit(f"x2_scaffold: {unit} .rodata family ends at {rodata['end']}, .bss starts at {h8(bss_start)}")
    # `.data` and `.bss` families cut per TU by link order (map_infer OVERLAY_DATA_SECTIONS): one splat
    # subsegment per TU piece, as `.rodata`; without them the section stays one unsplit piece.
    def family_rows(kind, low, high):
        family = next((f for f in layout.get("families", []) if f["family"] == kind), None)
        if not family or not family.get("owners"):
            return [[data_name, h8(low), h8(high)]] if low < high else []
        stem_of = {tu["ordinal"]: file_names[tu["ordinal"]] for tu in tus}
        rows = [[f"{unit}/{stem_of[p['ordinal']]}", h8(hx(p["start"])), h8(hx(p["end"]))] for p in family["owners"]]
        if hx(rows[0][1]) != low or hx(rows[-1][2]) != high:
            raise SystemExit(f"x2_scaffold: {unit} {kind} family {rows[0][1]}..{rows[-1][2]} is not {h8(low)}..{h8(high)}")
        return rows
    data_rows = family_rows("data", text_end, data_end)
    bss_rows = family_rows("bss", bss_start, section_end)
    for name, start, _ in data_rows:
        lines.append(f"      - [{up(hx(start) + base)}, data, {name}]")
    for name, start, _ in rodata_rows:
        lines.append(f"      - [{up(hx(start) + base)}, rodata, {name}]")
    lines.append(f"      - [{up(bss_start + base)}]")
    for name, start, _ in bss_rows:
        lines.append(f"      - {{ type: bss, vram: 0x{hx(start):08X}, name: {name} }}")
    lines.append(f"  - [{up(bss_start + base)}, pad, {unit}_bss_file_image]")
    shstrtab = image.sections[".shstrtab"]
    lines.append(f"  - [{up(shstrtab.offset)}, bin, elf_shstrtab]")
    lines.append(f"  - [{up(image.elf.shoff)}, bin, elf_section_headers]")
    lines.append(f"  - [{up(len(image.data))}]")
    out[f"config/split/{unit}.yaml"] = "\n".join(lines) + "\n"

    own = set(names.values())
    table = "".join(symbol_line(names[hx(f["va"])], hx(f["va"]), f["size"]) for f in functions)
    # Main's functions as this overlay's external names. An overlay's own name wins, so
    # a colliding main function is spelled by its address.
    externals = {va: (name if name not in own else f"func_{va:08X}")
                 for va, name in main_names.items()}
    table += "".join(symbol_line(name, va) for va, name in sorted(externals.items()))
    out[f"config/symbols/{unit}.txt"] = table + data_symbol_lines(unit)
    hand_data = OVERLAY_MAIN_DATA.get(unit, ())
    out[f"config/symbols/{unit}.main-symbols.ld"] = (
        "/* MAIN function symbols for overlay links (estimated, tools/tu/x2_scaffold.py) */\n"
        + "".join(f"{name} = 0x{va:08X};\n" for va, name in sorted(externals.items()))
        + "".join(f"{name} = 0x{va:08X}; /* {note} */\n" for name, va, note in hand_data)
        + overlay_main_data_lines(unit, own | set(externals.values()) | {name for name, _, _ in hand_data}))
    families = {".data": data_rows, ".rodata": rodata_rows, ".bss": bss_rows}
    out[f"config/objects/{unit}.layout.json"] = json_text(dict(
        provenance="estimated", text=layout_text, families=families,
        reference_witness_symbols={}, main_shadowed=[], reference_aliases={}))
    plan = dict(c_tus=[tu["ordinal"] for tu in tus])
    symbols = [[names[hx(f["va"])], hx(f["va"]), f["size"], 1] for f in functions]
    return entries, plan, symbols, file_names, names


# ---------------------------------------------------------------- main

# A second piece of an out-of-scope zone in one data section: (section, first byte, zone stem, piece name, why).
# A zone is one record with one piece per section, and Sony's libraries need two in .bss (zone audit 2026-11-19).
ZONE_EXTRA_PIECES = (
    (".bss", 0x00991000, "sce_sdk_low", "sce_sdk_low_bss2",
     "crt0's argument block (0x150 bytes) opens .bss at 0x69E400, and the libraries' own .bss follows the game's: "
     "from 0x991000 (first read by sceSifInitRpc; the scan pool of tu273 ends at 0x990FF0) to the CRI .bss at 0x9968C0"),
)


def data_cuts(tus, sections):
    """{section: [(start, end, tu record, entry, piece name or None)]}: each TU's piece runs to the next TU's."""
    out = {}
    extra_names = {}
    for sec in DATA_SECTIONS:
        section = sections[sec]
        lo, hi = section.addr, section.addr + section.size
        rows = []
        for tu in tus:
            entry = (tu.get("data") or {}).get(sec)
            if not entry or entry.get("anchored_start") is None:
                continue
            # A data object's section is 4-aligned, so an unaligned cut would move every
            # label of the piece. The bytes it gives up go to this TU, the later one.
            rows.append((hx(entry["anchored_start"]) & ~3, tu, entry))
        by_stem = {re.sub(r"\.s$", "", tu["suggested_name"]["name"]): tu for tu in tus if not tu["in_scope"]}
        extras = {}
        for section_name, start, stem, piece_name, _ in ZONE_EXTRA_PIECES:
            if section_name == sec:
                extras[start] = piece_name
                rows.append((start, by_stem[stem], dict(anchored_start=f"0x{start:08x}", anchored_end=None)))
        rows.sort(key=lambda row: (row[0], row[1]["ordinal"]))
        cuts, seen = [], set()
        for start, tu, entry in rows:
            # One piece per TU and per address: a later TU sharing a start owns nothing here.
            if start in seen or (start not in extras and tu["ordinal"] in {c[1]["ordinal"] for c in cuts
                                                                             if c[0] not in extras}):
                continue
            seen.add(start)
            cuts.append((max(start, lo), tu, entry))
        extra_names[sec] = extras
        if not cuts:
            out[sec] = []
            continue
        cuts[0] = (lo, cuts[0][1], cuts[0][2])
        pieces = []
        for k, (start, tu, entry) in enumerate(cuts):
            end = cuts[k + 1][0] if k + 1 < len(cuts) else hi
            pieces.append((start, end, tu, entry, extras.get(start)))
        out[sec] = pieces
    return out


def build_main(image, record, ov02_tus, ov02_stems, ov02_names, ov02_sizes, out):
    tus = record["tus"]
    S = image.sections
    used = stem_counts(tus)
    text = S[".text"]
    base = text.offset - text.addr
    names = unique_names([f for tu in tus for f in tu["functions"]])
    stems = {tu["ordinal"]: (re.sub(r"\.s$", "", tu["suggested_name"]["name"])
                             if not tu["in_scope"] else tu_file_name(tu, used)) for tu in tus}
    cuts = data_cuts(tus, S)

    bss_end = S[".bss"].addr + S[".bss"].size
    sdata_end_va = S[".sdata"].addr + S[".sdata"].size
    sdata_end_off = S[".sdata"].offset + S[".sdata"].size
    ov02 = S["ov02"]

    lines = yaml_options("main", image, False)
    lines += ["segments:", "  - [0x0, bin, blobs/elf_header]", "  - name: main", "    type: code",
              f"    start: {up(text.offset)}", f"    vram: 0x{text.addr:08X}",
              f"    bss_size: {up(bss_end - sdata_end_va)}", "    align: 8", "    subalign: null",
              "    subsegments:"]
    records = []
    for tu in tus:
        stem = stems[tu["ordinal"]]
        start, end = hx(tu["start"]), hx(tu["end"])
        category = tu["classification"]["cls"]
        hand = not tu["in_scope"]
        lines.append(f"      - [{up(start + base)}, {'hasm' if hand else 'c'}, {stem}]")
        optimization = tu.get("optimization") or "-O2"
        gp_count = image.gp_references(text, start, end)
        if hand:
            contract, evidence = asm_contract(), dict(
                g_flag="-G0", g_basis="whole library zone, kept as one hand-assembly scaffold",
                g_confidence="zone", source="tools/tu/x2_scaffold.py")
        else:
            g_flag, evidence = g_evidence(gp_count, optimization, start)
            contract = main_contract(optimization, g_flag)
        path = (f"scaffold/src/main/{stem}.s" if hand else f"src/main/{stem}.c")
        rec = dict(
            id=tu["id"], unit="main", ordinal=tu["ordinal"], name=stem, path=path,
            scaffold_path=f"scaffold/src/main/{stem}.{'s' if hand else 'c'}",
            kind="asm" if hand else "gcc", mode="asm", category="game" if tu["in_scope"] else category,
            map_class=category, domain=None, in_scope=bool(tu["in_scope"]),
            boundary_kind=tu["boundary_kind"], boundary_evidence=tu["boundary_evidence"],
            boundary_confidence=tu["boundary_confidence"], provisional=True,
            name_basis=(tu.get("suggested_name") or {}).get("basis"),
            name_confidence=(tu.get("suggested_name") or {}).get("confidence"),
            contract=contract, contract_evidence=evidence,
            accepted=dict(all_functions_accepted=False, functions=0,
                          of=len(tu["functions"]), migration_hints=[], records=[]),
            functions=[dict(binding="GLOBAL", name=names[hx(f["va"])], size=f["size"],
                            splat_name=names[hx(f["va"])], va=f["va"], name_basis=f.get("name_basis"),
                            provenance="estimated")
                       for f in tu["functions"]],
            owners={".text": "asm"},
            sections={},
            text=dict(start=h8(start), end=h8(end), code_end=tu.get("code_end"), size=end - start,
                      splat="hasm" if hand else "c", splat_range=[h8(start), h8(end)], note=None))
        records.append(rec)
    by_ordinal = {rec["ordinal"]: rec for rec in records}

    # .vutext, .reginfo, then .data: the gaps are zero padding the gate checks.
    vutext = S[".vutext"]
    reginfo = S[".reginfo"]
    lines.append(f"      - [{up(vutext.offset)}, databin, blobs/vutext]")
    lines.append(f"      - [{up(reginfo.offset)}, data, blobs/reginfo]")
    lines.append(f"      - [{up(reginfo.offset + reginfo.size)}, pad, blobs/pad_after_reginfo]")

    sections_out = {}

    def emit_section(sec):
        section = S[sec]
        order = []
        for start, end, tu, entry, extra_piece in cuts[sec]:
            rec = by_ordinal[tu["ordinal"]]
            stem = extra_piece or rec["name"]
            if sec in (".sbss", ".bss"):
                lines.append(f"      - {{ type: {SPLAT_TYPE[sec]}, vram: 0x{start:08X}, name: {stem} }}")
            else:
                lines.append(f"      - [{up(start - section.addr + section.offset)}, {SPLAT_TYPE[sec]}, {stem}]")
            rec["owners"][sec] = "asm"
            if extra_piece:
                # The zone's second piece: listed in the section order and on the record, whose own span stays.
                rec["sections"][sec]["pieces"].append(
                    dict(c_split=False, end=f"0x{end:08X}", name=stem, start=f"0x{start:08X}"))
                order.append(dict(c_split=False, end=f"0x{end:08X}", ordinal=tu["ordinal"], piece=stem,
                                  start=f"0x{start:08X}", tu=rec["name"]))
                continue
            rec["sections"][sec] = dict(
                anchored=[entry.get("anchored_start"), entry.get("anchored_end")],
                bounds=[entry.get("lower_bound_start"), entry.get("upper_bound_end")],
                end=f"0x{end:08X}", evidence_source="map", rule="anchored start",
                pieces=[dict(c_split=False, end=f"0x{end:08X}", name=stem, start=f"0x{start:08X}")],
                size=end - start, splat_type=SPLAT_TYPE[sec], start=f"0x{start:08X}")
            order.append(dict(c_split=False, end=f"0x{end:08X}", ordinal=tu["ordinal"], piece=stem,
                              start=f"0x{start:08X}", tu=stem))
        sections_out[sec] = dict(common_tail=None, end=f"0x{section.addr + section.size:08X}",
                                 order=order, start=f"0x{section.addr:08X}")

    emit_section(".data")
    ctors, dtors, eh_frame, vudata = S[".ctors"], S[".dtors"], S[".eh_frame"], S[".vudata"]
    lines.append(f"      - [{up(ctors.offset)}, data, blobs/ctors]")
    lines.append(f"      - [{up(dtors.offset)}, data, blobs/dtors]")
    lines.append(f"      - [{up(dtors.offset + dtors.size)}, pad, blobs/pad_after_dtors]")
    lines.append(f"      - [{up(eh_frame.offset)}, data, blobs/eh_frame]")
    lines.append(f"      - [{up(eh_frame.offset + eh_frame.size)}, pad, blobs/pad_after_eh_frame]")
    lines.append(f"      - [{up(vudata.offset)}, databin, blobs/vudata]")
    lines.append(f"      - [{up(vudata.offset + vudata.size)}, pad, blobs/pad_after_vudata]")
    emit_section(".rodata")
    rodata_end = S[".rodata"].offset + S[".rodata"].size
    lines.append(f"      - [{up(rodata_end)}, pad, blobs/pad_after_rodata]")
    emit_section(".lit4")
    lit4_end = S[".lit4"].offset + S[".lit4"].size
    lines.append(f"      - [{up(lit4_end)}, pad, blobs/pad_after_lit4]")
    emit_section(".sdata")
    lines.append(f"      - [{up(sdata_end_off)}]")
    emit_section(".sbss")
    sbss_end = S[".sbss"].addr + S[".sbss"].size
    if sbss_end < S[".bss"].addr:
        lines.append(f"      - {{ type: sbss, vram: 0x{sbss_end:08X}, name: blobs/sbss_end_gap }}")
    emit_section(".bss")

    lines.append(f"  - [{up(sdata_end_off)}, bin, blobs/pad_before_ov02]")
    lines += ["  - name: ov02", "    type: code", f"    start: {up(ov02.offset)}",
              f"    vram: 0x{ov02.addr:08X}", "    align: 4", "    subalign: null",
              "    subsegments:"]
    embedded = []
    embedded_names = []
    for k, entry in enumerate(ov02_tus):
        stem = f"ov02/{k:02d}_{ov02_stems[entry['ordinal']]}"
        start, end = (hx(x) for x in entry["text"])
        lines.append(f"      - [{up(start - ov02.addr + ov02.offset)}, c, {stem}]")
        embedded_names.append(stem)
        embedded.append(dict(
            id=f"main/ov02-embedded/tu{k:02d}", unit="main", ordinal=1000 + k, name=stem,
            path=f"scaffold/src/main/{stem}.c", scaffold_path=f"scaffold/src/main/{stem}.c",
            alias_of=entry["id"], kind="gcc", mode="asm", category="game",
            map_class="ov02-embedded", domain=None, in_scope=False,
            boundary_kind="alias", boundary_evidence=dict(note="same bytes as the OV02 unit's TU"),
            boundary_confidence="alias", provisional=True, name_basis=None, name_confidence=None,
            contract=main_contract("-O2", "-G0"),
            contract_evidence=dict(g_flag="-G0", g_basis="no gp-relative instruction in OV02"),
            accepted=dict(all_functions_accepted=False, functions=0, of=entry["functions"],
                          migration_hints=[], records=[]),
            functions=[], owners={".text": "asm"}, sections={},
            text=dict(start=entry["text"][0], end=entry["text"][1], size=end - start, section="ov02",
                      splat="c", splat_range=[entry["text"][0], entry["text"][1]], note=None)))
    ov02_text_end = max(hx(entry["text"][1]) for entry in ov02_tus)
    lines.append(f"      - [{up(ov02_text_end - ov02.addr + ov02.offset)}, data, ov02/data]")
    lines.append(f"  - [{up(ov02.offset + ov02.size)}, bin, blobs/dvp_ovlytab]")
    lines.append(f"  - [{up(S['.DVP.ovlystrtab'].offset)}, bin, blobs/dvp_ovlystrtab]")
    first_overlay = min(s.offset for s in image.elf.sections if s.name.startswith(".DVP.overlay"))
    lines.append(f"  - [{up(first_overlay)}, bin, blobs/dvp_overlays]")
    lines.append(f"  - [{up(S['.shstrtab'].offset)}, bin, blobs/elf_shstrtab]")
    lines.append(f"  - [{up(image.elf.shoff)}, bin, blobs/elf_section_headers]")
    lines.append(f"  - [{up(len(image.data))}]")
    out["config/split/main.yaml"] = "\n".join(lines) + "\n"

    fixed = [dict(name="blobs/vutext", section=".vutext", size=vutext.size, va=h8(vutext.addr),
                  note="VU microcode, retail databin until its programs are split"),
             dict(name="blobs/reginfo", section=".reginfo", size=reginfo.size, va=h8(reginfo.addr)),
             dict(name="blobs/ctors", section=".ctors", size=ctors.size, va=h8(ctors.addr)),
             dict(name="blobs/dtors", section=".dtors", size=dtors.size, va=h8(dtors.addr)),
             dict(name="blobs/eh_frame", section=".eh_frame", size=eh_frame.size, va=h8(eh_frame.addr)),
             dict(name="blobs/vudata", section=".vudata", size=vudata.size, va=h8(vudata.addr),
                  note="VU microcode, retail databin until its programs are split")]
    all_tus = records + embedded
    manifest = dict(
        schema="tu-manifest-main/2", unit="main", analysis_only=False,
        task="x2-scaffold", owner="tools/tu/x2_scaffold.py",
        provenance="estimated", identity=dict(binary=image.file, sha256=image.sha256),
        cut_rule=("provisional; byte identity does not depend on it: the first object of a section "
                  "starts at the section start, every later object at its first anchored reference, "
                  "lowered to a 4-byte boundary"),
        auto_splits=dict(file="auto-splits.json", used={}),
        fixed_blobs=fixed, ov02_embedded=embedded_names, sections=sections_out,
        symbols=dict(added=[], aliases=[], conflicts=[], renamed=[]),
        text_order=[rec["name"] for rec in records], tus=all_tus)
    out["config/objects/main.objects.json"] = json_text(manifest)
    out["config/split/main.data-splits.json"] = json_text(dict(
        schema="tu-auto-splits-main/1", tus={}, detail={}, problems=[]))
    out["config/objects/main.compile.json"] = json_text(dict(
        schema="tu-compile-manifest-main/2", unit="main",
        source="tu-manifest.json (generated together)",
        toolchains={
            COMPILER_ID: dict(cc_dir=COMPILER_DIR, cc1_sha256=CC1_SHA, note=CONTRACT_BASIS),
            "LA29-vsqrt": dict(path="tool:ee-as-la29-vsqrt", sha256=AS_PLAIN_SHA,
                               use="runtime and hand-written objects"),
            "LA29-vsqrt-swap-g": dict(path="tool:ee-as-la29-vsqrt-swap", sha256=AS_SWAP_SHA,
                                      assembler_options=["-g"], use="game C TUs"),
        },
        tus={rec["name"]: dict(
            id=rec["id"], ordinal=rec["ordinal"], path=rec["path"], kind=rec["kind"],
            category=rec["category"], in_scope=rec["in_scope"], mode=rec["mode"],
            language=rec["contract"]["language"], compiler_profile=rec["contract"]["compiler_profile"],
            flags=rec["contract"]["flags"], assembler=rec["contract"]["assembler"],
            owners=rec["owners"], evidence=rec["contract_evidence"].get("source"),
            all_functions_accepted=False, accepted_functions=f"0/{rec['accepted']['of']}",
            **({"assembler_options": rec["contract"]["assembler_options"]}
               if rec["contract"].get("assembler_options") else {}))
             for rec in all_tus}))
    table = "".join(symbol_line(names[hx(f["va"])], hx(f["va"]), f["size"])
                    for tu in tus for f in tu["functions"])
    own = set(names.values())
    for va, name in sorted(ov02_names.items()):
        # OV02's functions at their shared VAs, by address where main spells a name otherwise.
        table += symbol_line(name if name not in own else f"func_{va:08X}", va)
    out["config/symbols/main.txt"] = table + data_symbol_lines("main")
    out["config/symbols/main.externals.ld"] = (
        "/* symbols accepted unit records take from other units (witnessed overlay addresses) */\n")
    symbols = [[names[hx(f["va"])], hx(f["va"]), f["size"], text.index]
               for tu in tus for f in tu["functions"]]
    symbols += [[name, va, ov02_sizes[va], ov02.index] for va, name in sorted(ov02_names.items())]
    return symbols, {va: names[va] for tu in tus for va in (hx(f["va"]) for f in tu["functions"])}


def keep_published_main_tus(out):
    """Write the MAIN import registry and put its C TUs back: regenerating must not unpublish them.

    Same shape as tools/review_stage.py apply_mode_c, which writes these entries at publication.
    """
    path = ROOT / "config/tu/main/imports.json"
    imports = (json.loads(path.read_bytes()) if path.is_file() else
               dict(schema="tu-imports-main/1",
                    about="Per-TU build directives for the MAIN unit, keyed by TU ordinal. Publication adds "
                          "an entry when a TU turns to C (tools/review_stage.py apply_mode_c).",
                    tus={}))
    out["config/tu/main/imports.json"] = json_text(imports)
    if imports["tus"]:
        objects = json.loads(out["config/objects/main.objects.json"])
        for tu in objects["tus"]:
            entry = imports["tus"].get(str(tu["ordinal"]))
            if entry:
                owners = entry.get("owners") or {}
                tu["import"] = dict(mode="c", note=entry.get("note"), source=entry["source"],
                                    **({"owners": owners} if owners else {}))
                tu["mode"] = "c"
                tu.setdefault("owners", {}).update({".text": "c", **owners})
        out["config/objects/main.objects.json"] = json_text(objects)


def apply_reviewed_names(unit, tus):
    """Names chosen after the map was inferred: {"units": {unit: {va: name}}}.

    A name is a label, never identity (decision D1), so changing one here and
    regenerating is the whole rename route. A row for an address the map does not
    have stops the generator, because it would otherwise be silently lost.
    """
    if not REVIEWED_NAMES.is_file():
        return
    wanted = json.loads(REVIEWED_NAMES.read_bytes()).get("units", {}).get(unit) or {}
    functions = {hx(f["va"]): f for tu in tus for f in tu["functions"]}
    for va, name in wanted.items():
        if hx(va) not in functions:
            raise SystemExit(f"{REVIEWED_NAMES}: {unit} has no function at {va}")
        functions[hx(va)]["name"] = name


def generate():
    out = {}
    main_map = json.loads(MAIN_MAP.read_bytes())
    ovl_map = json.loads(OVL_MAP.read_bytes())["overlays"]
    apply_reviewed_names("main", main_map["tus"])
    for unit in OVERLAYS:
        apply_reviewed_names(unit, ovl_map[unit]["tus"])
    images = {unit: Image(unit) for unit in FILES}
    main_functions = [f for tu in main_map["tus"] for f in tu["functions"]]
    main_names = unique_names(main_functions)
    overlay_units = {}
    plan_units = {}
    table_units = {}
    stems, names_of = {}, {}
    for unit in OVERLAYS:
        entries, plan, symbols, stems[unit], names_of[unit] = build_overlay(
            unit, images[unit], ovl_map[unit], main_names, out)
        overlay_units[unit] = dict(tus=entries)
        plan_units[unit] = plan
        table_units[unit] = dict(file=FILES[unit], sha256=images[unit].sha256, functions=symbols)
    ov02_sizes = {hx(f["va"]): f["size"] for tu in ovl_map["ov02"]["tus"] for f in tu["functions"]}
    symbols, _ = build_main(images["main"], main_map, overlay_units["ov02"]["tus"], stems["ov02"],
                            names_of["ov02"], ov02_sizes, out)
    table_units["main"] = dict(file=FILES["main"], sha256=images["main"].sha256, functions=symbols)
    # Main code calls into OV01 at its load address (battleSlotsRefresh from DispMapObject). Named
    # OV01 functions are offered to main's C as PROVIDE()s; a main or OV02 name always wins.
    # Address labels (func_00A9B7D8, called by textDrawAll) are offered too: a hand edit of the
    # output was lost at the next regeneration (2026-11-08).
    taken = set(main_names.values()) | set(names_of["ov02"].values())
    out["config/symbols/main.externals.ld"] += "".join(
        f"PROVIDE({name} = 0x{va:08X}); /* ov01 */\n" for va, name in sorted(names_of["ov01"].items())
        if name not in taken)
    out["config/symbols/main.externals.ld"] += "".join(
        f"PROVIDE({name} = 0x{va:08X}); /* libgcc */\n"
        for va, name in sorted(SOFT_FLOAT_LIBCALLS.items()))
    out["config/symbols/main.externals.ld"] += "".join(
        f"PROVIDE({name} = 0x{va:08X}); /* {note} */\n" for name, va, note in EXTRA_PROVIDES)
    out["config/symbols/estimated-functions.json"] = json_text(dict(
        schema="x2-estimated-functions/1", provenance="estimated",
        note=["Function starts and sizes inferred by tools/tu/map_infer.py, names from annotations",
              "or the address. Each row is [name, va, size, section index]. Not a symbol table:",
              "the originals have none, and every binding is assumed GLOBAL."],
        units=table_units))
    out["config/objects/overlays.compile.json"] = json_text(dict(
        schema="ovl-tu-compile-manifest/1", task="x2-scaffold", identity="tools/tu/x2_scaffold.py",
        compiler_default=compiler_record(),
        assemblers=dict(
            c_tu=dict(id=AS_SWAP_NAME, path="tool:ee-as-la29-vsqrt-swap", sha256=AS_SWAP_SHA,
                      argv="-EL -m5900 -mabi=eabi <G> -g -I .", basis=CONTRACT_BASIS)),
        units=overlay_units))
    out["config/objects/iop.json"] = json_text(dict(
        schema="iop-objects/1",
        note=["The IOP modules are out of scope (user decision of 2026-11-05: only game code).",
              "tools/coverage_report.py still reads this file, so it states an empty scope."],
        units={}, groups=[]))
    keep_published_main_tus(out)
    plan = json.loads((ROOT / "config/overlay-plan.json").read_bytes())
    plan["units"] = plan_units
    plan["note"] = ["Which original overlay objects are built from a C translation unit.",
                    "Generated by tools/tu/x2_scaffold.py: every TU of both overlays."]
    out["config/overlay-plan.json"] = json_text(plan)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="report differences, write nothing")
    a = ap.parse_args(argv)
    outputs = generate()
    changed = []
    for relative, text in sorted(outputs.items()):
        path = ROOT / relative
        if not path.is_file() or path.read_text() != text:
            changed.append(relative)
            if not a.check:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text)
    print(json.dumps(dict(files=len(outputs), changed=changed), indent=1))
    return 1 if a.check and changed else 0


if __name__ == "__main__":
    raise SystemExit(main())
