# Xenosaga Episode II

[![Code](https://bytematch.dev/OpokXeno/xsg-ii-decomp.svg?mode=shield&label=Code)](https://bytematch.dev/OpokXeno/xsg-ii-decomp)
[![Functions](https://bytematch.dev/OpokXeno/xsg-ii-decomp.svg?mode=shield&label=Functions&measure=matched_functions)](https://bytematch.dev/OpokXeno/xsg-ii-decomp)

> This project is developed with AI assistance.

A matching decompilation of **Xenosaga Episode II: Jenseits von Gut und Böse**
for the PlayStation 2.

The goal is readable C source that reproduces the original machine code.
The Sony SDK and public libraries do not need to be recovered.

## Progress

Recovered 5,057 of 5,057 in-scope game function(s) (100.000%).

| Version | Target | Functions | Progress |
| --- | --- | ---: | ---: |
| NTSC-U | `SLUS_208.92` | 3,776 / 3,776 | 100.000% |
| NTSC-U | `OV01.OVL` | 1,259 / 1,259 | 100.000% |
| NTSC-U | `OV02.OVL` | 22 / 22 | 100.000% |

## Building

The historical compilers are 32-bit Linux executables. On 64-bit Linux,
install `libc6-i386` and `python3-venv`. On Windows, use WSL and keep the
checkout and build on a Linux filesystem: the old compilers cannot read some
Windows filesystem metadata and report `Value too large for defined data type`.

You need three things the repository does not contain: your own copy of the
game, the original toolchain, and a Python environment for the disassembler.

### 1. The originals

Extract `SLUS_208.92`, `OV01.OVL`, `OV02.OVL` and the `IOP/` directory from your own
disc image and put them in one folder. Either disc works, because these files are
byte-identical on both (disc 2 boots `SLUS_211.33`, a copy of `SLUS_208.92`).
`config/originals.json` names the three files with their sizes and SHA-256; the build
refuses anything else, so it tells you immediately if you have a different release.
This project reproduces the NTSC-U release.

### 2. The toolchain

`config/toolchain-identity.json` gives, for every compiler and assembler, the version,
the SHA-256 of each payload, and the provenance needed to obtain or rebuild it. These
tools were identified on Xenosaga Episode I, and they fit this game on everything measured
so far.

| Tool | What the build uses it for | Where it comes from |
| --- | --- | --- |
| `ee-gcc2.96-realconv-lp7` | `cc1` for the game code and the C runtime | the published `ee-gcc2.96` archive, with documented patches to `cc1` |
| `ee-gcc2.9-991111` | `cc1` for the SCE SDK scaffold objects only (out of recovery scope) | the published `ee-gcc2.9-991111` archive, unmodified |
| `ee-as-la29-vsqrt` | assembler for every game and runtime object | GNU as 2.9-ee-991111 built from the pinned `ps2-ee-toolchain` commit with recorded fixes |
| `ee-as-2.9-plain` | assembler for the SCE SDK objects | the same commit, unpatched |
| `ps2dev-binutils` | linker and objcopy of every image, and modern GAS for data objects and retail blobs | ps2dev binutils 2.45.1 |

Lay them out under one directory as `config/toolchain-identity.json`'s
`tools.<id>.dir` / `.path` say. Then:

```bash
python3 -B tools/tu/toolchain.py --check
```

It prints where it looked and what it found, and refuses with the tool's name,
both hashes and the recipe when something is missing or different.

### 3. The disassembler

```bash
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
```

### 4. Tell the build where those three are

By environment variable:

```bash
export XENO_GAME_DIR=/path/to/extracted-iso
export XENO_TOOLCHAIN_DIR=/path/to/toolchains
export XENO_SPLAT_PYTHON=$PWD/.venv/bin/python
```

or once, in `config/toolchain-local.json` (untracked):

```json
{
  "game_dir": "/path/to/extracted-iso",
  "toolchain_root": "/path/to/toolchains",
  "splat_python": "/path/to/.venv/bin/python"
}
```

Three of the tools are built from source rather than downloaded, so their bytes
depend on the host compiler. A build of the same source that assembles the same
bytes still reproduces the images: accept it one tool at a time with
`XENO_TOOLCHAIN_ALLOW=ee-as-la29-vsqrt,ee-as-2.9-plain,ps2dev-binutils`. The
build then says which check it relaxed.

### 5. Build

```bash
./configure.py      # generate every unit build directory and build.ninja
ninja               # build and compare all three originals
ninja gate          # + per-unit status and the whole-file SHA-256 gate
```

`ninja gate` writes `build/gate.json`. `"result": "pass"` means every one of the
three files came out byte for byte identical to your original.

## Collaborating
We have a private Trello board where we divide up the work so we don't step on each other's toes.
If you're interested, contact me on Discord. I'm opok (opok#2874).

## Acknowledgments

Thanks to the developers and contributors of the projects on which this work relies:

- [splat](https://github.com/ethteck/splat),
  [spimdisasm](https://github.com/Decompollaborate/spimdisasm), and
  [Rabbitizer](https://github.com/Decompollaborate/rabbitizer) for splitting and disassembly.
- [m2c](https://github.com/matt-kempster/m2c) for first-pass decompilation of MIPS
  assembly into C.
- [decomp-permuter](https://github.com/simonlindholm/decomp-permuter) for the bounded
  mutation search over a compiled non-exact base.
- [ps2dev/binutils-gdb](https://github.com/ps2dev/binutils-gdb) and
  [SSXModding/ps2-ee-toolchain](https://github.com/SSXModding/ps2-ee-toolchain) for EE toolchains.
- [decomp.me's compiler collection](https://github.com/decompme/compilers) for historical compiler distributions.
- [PCSX2](https://github.com/PCSX2/pcsx2) for its documented EE implementation and behavioral reference.

## Disclaimer

A legally obtained copy of the game is required. No original game binaries or
copyrighted game assets are distributed in this repository. All game rights belong
to their respective owners; this project is not affiliated with or endorsed by them.
