# Native Metrowerks compilers for PSP and PS2

Decompiled Metrowerks CodeWarrior C/C++ compilers for PlayStation Portable and PlayStation 2, built as native 64-bit
programs (x86-64 or arm64, macOS or Linux) that produce the same object files as the originals. Decompilation projects
can use them instead of running the Windows executables under wibo or Wine.

| Compiler | Version | Used by | Release tag | Exact |
| -------- | ------- | ------- | ----------- | ----- |
| `mwccpsp` | PSP 3.0.1 build 219 | [Castlevania: Symphony of the Night](https://github.com/Xeeynamo/sotn-decomp) | `mwccpsp-*` | ![mwccpsp](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/sozud/mwcc-info/main/mwccpsp.json) |
| `mwccps2` | PS2 3.0.1 build 210 | Persona 4 | `mwccps2-*` | ![mwccps2 b210](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/sozud/mwcc-info/main/mwccps2.json) |
| `mwccps2` b52 | PS2 3.0 build 52 | [Monster Hunter](https://github.com/itsbigcforme67/mh1-xbox) | `mwccps2-b52-*` | ![mwccps2 b52](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/sozud/mwcc-info/main/mwccps2-b52.json) |
| `mwccps2` b69 | PS2 build 69 (version string 3.0.3) | Silent Hill 3 | `mwccps2-b69-*` | ![mwccps2 b69](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/sozud/mwcc-info/main/mwccps2-b69.json) |
| `mwccps2` b198 | PS2 3.0.1 build 198 | | `mwccps2-b198-*` | ![mwccps2 b198](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/sozud/mwcc-info/main/mwccps2-b198.json) |

Each release has `<compiler>-native64-<os>-<arch>.tar.gz` for linux-amd64, darwin-amd64 and darwin-arm64.

"Exact" is the share of the original executable's function bytes that our source reproduces byte for byte (from
[sozud/mwcc-info](https://github.com/sozud/mwcc-info)). The natives match the originals' output on everything their
corpora reach well beyond that: functions that aren't byte-exact are still written to behave identically, which the
corpora below check.

## How they are built

All five come from one set of sources (`src/`, `include/`), a fork of [doldecomp/mweppc](https://github.com/doldecomp/mweppc),
the decompilation of CodeWarrior for GameCube. A build picks its version with defines:

- `MWCCPSP`: the PSP compiler's changes to the shared front end and optimizer; its MIPS back end, driver and the
  optimizer passes 3.0.1 rewrote are in `src/psp/`.
- `MWCCPS2` (with `MWCCPSP`): PS2 3.0.1 build 210 (a 128-bit `CInt64`, its options, the R5900 opcode table and
  instruction set, assert line numbers); `MWCCPS2_BUILD` names the build. `MWCCPS2_BUILD=198` is build 198: where it
  differs from 210 the code is under `MWCCPS2_BUILD < 198` / `>= 198` (it has b210's code there) or `< 210` (it has the
  older code, as b145 does), and its own assert lines are `CError_B198_SHIFT`.
- `MWCC_CORE` (`include/compiler/mwcc_core.h`): the older PS2 builds 69 and 52, ordered by their code (b69, then b52,
  then 3.0.1). Their differences are guarded by position: `#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52` for what
  both have, `MWCC_CORE < MWCC_CORE_B52` for b69 only, `MWCC_CORE == MWCC_CORE_B52` for b52 only. `-DMWCCPS2_B69=1`
  or `-DMWCCPS2_B52=1` selects the position; for every other build `MWCC_CORE` is 0.

The native build compiles the units with clang (`-fwrapv -fno-strict-aliasing -fsigned-char -fshort-enums`, LP64
paths under `MWCCPSP_NATIVE`), and takes what the sources don't define (parser tables, strings, data tables) from the
original executable at build time, using the symbol files' addresses. What no test reaches may still be missing: the
program stops with "not implemented" at the first function without source.

## mwccpsp (PSP 3.0.1 build 219)

```sh
mkdir -p orig/PSP_3_0_1 && cp /path/to/mwccpsp.exe orig/PSP_3_0_1/
python3 tools/mwccpsp/native_build.py -o build/PSP_3_0_1/mwccpsp
```

or in Docker, which downloads the original itself:

```sh
docker build -f tools/mwccpsp/Dockerfile -t mwccpsp .
docker run --rm -v "$PWD/build:/out" mwccpsp
```

The data tables' addresses are in `config/PSP_3_0_1/symbols*.txt`. Everything the Symphony of the Night PSP build
reaches gives identical objects at every optimization level.

## mwccps2 (PS2 3.0.1 build 210)

```sh
python3 tools/mwccps2/get_orig.py      # orig/PS2_3_0_1/mwccps2.exe, from decomp.me's public image, checked by digest
python3 tools/mwccpsp/native_build.py --ps2 -o build/PS2_3_0_1/mwccps2
```

or `docker build -f tools/mwccps2/Dockerfile -t mwccps2 .`. `tools/mwccps2/native_ps2.py` regenerates the PS2 data
tables in `src/native/ps2/` (CI checks they come out as committed); the sources name PSP addresses, so the build maps
names through `config/PS2_3_0_1/symbols.txt` and `config/PS2_3_0_1/addrmap.csv` (`tools/mwccps2/ps2_map.py`).

Every unit of the Persona 4 decompilation (1,008 C files, RenderWare at `-O4,p`) gives identical objects except
`g_data.c`, which the original itself doesn't reproduce from its captured inputs.

## mwccps2 build 198 (PS2 3.0.1 build 198, 2005-10-11)

```sh
python3 tools/mwccps2/b198/get_orig.py      # orig/PS2_3_0_1_b198/, from decomp.me's public image, checked by digest
python3 tools/mwccps2/b198/native_b198.py -o build/PS2_3_0_1_b198/mwccps2
```

or `docker build -f tools/mwccps2/b198/Dockerfile -t mwccps2-b198 .`. The same sources as b210 with
`-DMWCCPS2_BUILD=198`; `native_b198.py --gen` regenerates its data tables in `src/native/ps2/b198/`, and the build maps
names through `config/PS2_3_0_1b198/native/` (generated from the research tree's b198 name map).

Its corpus is every PS2 corpus above replayed through the original b198 (7,499 cases: Persona 4, Persona 3 FES,
Yakuza, Monster Hunter, Silent Hill 3, Yu-Gi-Oh! Duelists of the Roses). The native gives identical objects for 6,614.
The other 885 depend on b198 reading memory it never initialised: 40 the original itself doesn't reproduce from one work
directory to another, and 845 follow an operand's rclass/flags left over on the stack (`func_004C3C60` never writes
them; copy propagation tests `flags & 8`). Replaying the words the original reads there into the native makes all 845
match (`tools/mwccps2/b198/residue/` and `tools/mwccps2/b198/corpus-ub.txt`).

## mwccps2 builds 69 and 52 (Silent Hill 3, Monster Hunter)

```sh
python3 tools/mwccps2/b69/get_orig.py   # orig/PS2_3_0_3_b69/, orig/PS2_3_0_1_b145/: decomp.me's images, checked by digest
python3 tools/mwccps2/b69/native_b69.py -o build/PS2_3_0_3_b69/mwccps2
python3 tools/mwccps2/b52/get_orig.py   # orig/PS2_3_0_b52/, orig/PS2_3_0_1_b145/
python3 tools/mwccps2/b52/native_b52.py -o build/PS2_3_0_b52/mwccps2
```

or `docker build -f tools/mwccps2/b69/Dockerfile -t mwccps2-b69 .` (or `b52`). Build 69 is the GameCube 2.x front end
and optimizer with mwccps2's MIPS back end; build 52 is build 69's code plus 3.0.1's IRO optimizer (with eight passes
not there yet), C99 designated initializers and GCC statement expressions. `tools/mwccps2/native_core.py` builds
either from `tools/mwccps2/<build>/units.txt` (each unit with its defines: no `MWCCPSP` for the GameCube-era units,
`MWCCPSP MWCCPS2 MWCCPS2_BUILD=69` for the back end, `MWCCPSP` for b52's IRO optimizer). Units only these builds have
are in `src/native/b69/` or excluded from the PSP build in `config/PSP_3_0_1/psp-exclude.txt`. What the units leave
undefined is resolved from each build's `glue/` and `symbols*.txt`: parser tables and strings from its own exe, the
copied back end's data tables from b145.

- b52: every unit of the Monster Hunter corpus (4,607 cases: the mh1-xbox decompilation's PS2 units for each
  version, and the Persona 4 units replayed through b52) gives identical objects, and so does mh1-xbox's own build;
  18,430 of 18,434 of those cases recompiled at `-O0` to `-O4,p` (the 4 left are `g_data.c`), and 3,836 of 3,838
  cases recaptured with b52 from Silent Hill 3, Persona 4, Yu-Gi-Oh! Duelists of the Roses and Persona 3 FES.
- b69: 1,029 of the 1,084 Persona 4 units compiled by b69 give identical objects. The other 55 depend on b69 reading
  memory it never initialised (a constant node's `hascall` byte left over from earlier allocations, and an operand's
  class byte on the stack), so no rebuild can reproduce them deterministically; `tools/mwccps2/b69/corpus-ub.txt`
  lists them with the evidence (replaying the bytes b69 read makes all 55 match).
- C++ is largely untested on both.

## Testing

`tools/mwccpsp/test.py` compiles the files in `tools/mwccpsp/tests/` with a native build and the original and
compares the objects (`tests/known-failures-<psp|ps2>.txt` lists what isn't reproduced yet):

```sh
python3 tools/mwccpsp/test.py --cc build/PSP_3_0_1/mwccpsp --orig "wibo orig/PSP_3_0_1/mwccpsp.exe"
python3 tools/mwccpsp/test.py --ps2 --cc build/PS2_3_0_1/mwccps2 --orig "wibo orig/PS2_3_0_1/mwccps2.exe"
```

The "Exact" badges come from `tools/exact/exact.py`: it compiles each compiler's units for Windows x86 with the
CodeWarrior version and flags the original was built with (CW 9.2 for b219 and b210; Pro 6, 5.3 and 5 for b69 and
b52, under wibo), compares every function placed at an original address with the original's bytes (relocation slots
masked, every relocation checked to point at the right target), and counts the bytes of the original's function table
(`functions.csv`) that match. The units and flags are in each compiler's `exact_units.json`; the host compilers come
from the `compilers` branch of [sozud/mwcc-info](https://github.com/sozud/mwcc-info/tree/compilers):

```sh
git clone -b compilers https://github.com/sozud/mwcc-info build/mwcc-compilers
python3 tools/exact/exact.py --json-out build/exact/badges          # or: exact.py b69 mwccpsp ...
```

On every push to `mwccpsp`, `.github/workflows/exact.yml` runs it and commits the badges (`mwccpsp.json`,
`mwccps2.json`, `mwccps2-b52.json`, `mwccps2-b69.json`) to mwcc-info's main branch when they change.

Pushing a release tag (table above) builds that compiler for each platform in GitHub Actions and attaches the
binaries to a release (`.github/workflows/<compiler>-release.yml`).

## Where the code comes from

The compilers are matched function by function against the original executables in the
[Castlevania: Symphony of the Night decompilation](https://github.com/Xeeynamo/sotn-decomp) (branches
`mwccpsp-decomp` and `mwccps2-decomp`, under `tools/mwccpsp/` and `tools/mwccps2/`), which started from mweppc at
`919828d`. `tools/mwccpsp/sync_sotn.py` brings that tree over: changes to files mweppc has go behind `#if MWCCPSP`
with the original lines in the `#else`, new files are copied, and the result is clang-formatted.

| sotn-decomp `tools/mwccpsp/`                                        | here                                          |
| ------------------------------------------------------------------- | --------------------------------------------- |
| `src/**`, `include/**`                                              | `src/**`, `include/**` (guarded as above)     |
| `re/symbols.txt`, `re/symbols.manual.txt`, `config/psp-exclude.txt` | `config/PSP_3_0_1/`                           |
| `harness/{native,native_build,native_glue,pe,lint64}.py`            | `tools/mwccpsp/` (paths rewritten)            |
| `re/gen_{cc_psp,native_data,native_strings,opcode_table,parser_psp}.py` | `tools/mwccpsp/`                          |
| `../mwccps2/harness/native_ps2.py`                                  | `tools/mwccps2/native_ps2.py`                 |
| `../mwccps2/re/addrmap.csv`                                         | `config/PS2_3_0_1/addrmap.csv`                |

```sh
python3 tools/mwccpsp/sync_sotn.py ../sotn-decomp --rev <commit> [--ps2]
```

Builds 69 and 52 come from the same tree's `tools/mwccps2/b69/` (one source tree for both, b52 adding to b69), merged
here under the `MWCC_CORE` guards with each build's view checked against that tree's. See
[docs/mwccps2-regression.md](docs/mwccps2-regression.md) for how the PSP compiler was checked across the PS2 work.

## Contributing

- Format C with `clang-format -i`, and Python with `ruff format` and `ruff check` (the venv's pinned versions); CI
  checks both and builds every version with clang.
- Braces on every `if`, `else`, `for`, `while` and `do` body; flags are `TRUE` and `FALSE`.
- Keep each change behind the guard of the builds it belongs to, and check the others still compile what they did:
  the natives' code sections should not change for a build a change doesn't target.

## Credits

- [doldecomp/mweppc](https://github.com/doldecomp/mweppc), the CodeWarrior for GameCube decompilation this is a fork
  of, and through it Ninji's decompilation of the CodeWarrior Pro 7 Mac OS X compiler,
  [rayanht/mwcc](https://github.com/rayanht/mwcc) and
  [henriquegemignani/mwcc](https://github.com/henriquegemignani/mwcc).
- [decomp.me](https://decomp.me)'s compiler images (the originals the builds read their data from),
  [wibo](https://github.com/decompals/wibo) and [objdiff](https://github.com/encounter/objdiff).

## License

This decompilation is dedicated to the public domain (CC0, `LICENSE`).
