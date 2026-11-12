# Adding mwccps2: checking the PSP compiler didn't regress

The `mwccps2` branch adds the PS2 compiler on top of `mwccpsp` in steps that can be checked one at a time:

| Commit    | What                                                                                        |
| --------- | ------------------------------------------------------------------------------------------- |
| `ec2458f` | base: `origin/mwccpsp`, the PSP snapshot (sotn-decomp `4f5dba132`)                          |
| `3871666` | `tools/mwccpsp/sync_sotn.py`, which reproduces the snapshot's sources exactly from `4f5dba132` |
| `ba47f6f` | PSP refresh: sotn-decomp `mwccpsp-decomp` `5da42df11`, the commit `mwccps2-decomp` merged    |
| `6a2f813` | PS2 addition: sotn-decomp `mwccps2-decomp` `db94925b5`                                      |
| `c72417c` | fix for a PSP regression that came with `db94925b5` (below)                                 |
| later     | test tooling, CI, docs                                                                      |
| `1e2c7bc` | refresh from sotn-decomp `mwccps2-decomp` `988196355` (has the same fix; see the end)       |

All results below are from an x86-64 Mac (clang 17), every run at `-j 1`.

## The GameCube builds

`sync_sotn.py` puts every change to a file `919828d` has behind `#if MWCCPSP`. For each of the 473 C/C++ files `919828d`
has, `unifdef -UMWCCPSP -UMWCCPSP_NATIVE -UMWCCPSP_TRACE -UMWCCPS2 -UMWCCPS2_BUILD` of the branch's file gives the same
tokens as `919828d`'s (whitespace aside: the merged files are clang-formatted), after the PSP refresh and after the PS2
addition: 473/473.

## `tools/mwccpsp/test.py` (native against the original under wibo)

| Build                                 | Result                                            |
| ------------------------------------- | ------------------------------------------------- |
| mwccpsp, base `ec2458f`               | 177/360 identical                                 |
| mwccpsp, PSP refresh `ba47f6f`        | 342/360 identical                                 |
| mwccpsp, PS2 addition `6a2f813`       | 48/360 identical, `unary.cpp` hangs at `-O4,*`    |
| mwccpsp, with the fix `c72417c`       | 342/360 identical (the same 18 as the refresh)    |
| mwccps2, `c72417c` (`--ps2`)          | 263/452 identical                                 |

Every case the refresh gets wrong, the base gets wrong too. The 18 (`tests/known-failures-psp.txt`): `initarray.c`
(7 flag sets) and `exceptions_templ.cpp` (7), whose objects hold uninitialised bytes (below), and `dwarf.c`,
`dwarf_cpp.cpp` with `-g` (4).

The 189 PS2 cases (`tests/known-failures-ps2.txt`): 27 the original rejects too (PSP/VFPU inline assembler,
`escapes.c`); the rest are C++ the Persona 4 corpus (C only) never reaches ("not implemented" stubs for
`ps2_CExpr_UnaryBuiltin`, `ps2_CExpr_MatchOperands`, `ps2_CExpr_BinaryBuiltin`; an internal error in
`CTemplateTools.c`), debug info and `initarray.c`.

### The regression in `db94925b5`

sotn-decomp `b51453c1a` ("Merge branch 'mwccps2-native' into mwccps2-decomp") made `MIPS_Peephole.c` read the live
register sets from `data_0062541C` whenever `MWCCPSP_NATIVE` is set, which the native PSP build sets too. The PSP build
then gets almost every test wrong (even `int f(int x) { return x + 1; }` at `-O0`). sotn-decomp's own native PSP build
at `db94925b5` gives the same objects as the branch's, so the export isn't the cause; bisecting `mwccps2-decomp`'s
first-parent history (40653b72d good, 9a9fadc1b bad; the merge itself doesn't build, 9a9fadc1b repairs it) and
diffing the PSP view of the sources between them narrows it to that one macro. `c72417c` limits it to `MWCCPSP_NATIVE && MWCCPS2`; the PS2 build compiles
the same code as before. sotn-decomp has had the same fix since `ca2f6939f`, so `sync_sotn.py` no longer brings the bug back.

## `tools/mwccpsp/difftest.py` (two builds against each other)

Inputs: the tests with every flag set of `test.py` (both compilers' sets), and this repository's own 470 C files at
`-O0`, `-Op`, `-O4,p`, `-O4,s`: 1,716 compiles. Where the two builds differ, the original under wibo decides
(`--orig`).

| A → B                      | Result                                                                                         |
| -------------------------- | ---------------------------------------------------------------------------------------------- |
| base → PSP refresh         | 0 regressions: 1,196 fixed, 212 same, 226 same error, 28 both crash, 36 B no longer crashes but isn't the original's, 18 changed (neither is the original's) |
| PSP refresh → `c72417c`    | 0 regressions: 1,235 same, 443 same error, 38 changed                                          |
| `c72417c` → `c72417c`      | 40 differ from run to run                                                                      |
| PSP refresh → `c72417c`, with the nondeterminism check (the final tool) | exit 0: 1,234 same, 443 same error, 39 nondeterministic |

The "changed" cases are all among the 40 that differ from one run of the same binary to the next: `initarray.c` and
`exceptions_templ.cpp` (every flag set), and `InlineAsmPPC.c`, `TOC.c`, `CRTTI.c`, `IroLinearForm.c`,
`IroPointerAnalysis.c`, `IroTransform.c`, `IroUtil.c` from `src/`. The native compilers write uninitialised heap bytes
into these objects (in `initarray.c`, a pointer's worth of bytes in `.data`); the base build does the same. difftest
now compiles a case twice with A and skips it ("nondeterministic") when the two differ.

## Golden corpora (local only: the inputs aren't public)

Run from the sotn-decomp checkouts with their `tools/mwccpsp/corpus/run_corpus.py` and `--cc` the branch's native
build (`c72417c`):

| Corpus                                                  | Result |
| ------------------------------------------------------- | ------ |
| PSP `build/mwccpsp-corpus` (SOTN's PSP objects)         | 2044/2044 identical |
| PSP `build/mwccpsp-corpus-ext` (the same at `-O0`..`-O4,p`) | 10198/10198 identical |
| PS2 `build/mwccps2-corpus` (Persona 4), mwccps2 from `6a2f813` and again from the tip | 1007/1008 identical; `g_data.c` differs, as it does with the original replaying it |
| PS2 `build/mwccps2-corpus`, mwccps2 from `1e2c7bc` (macOS, and the Docker Linux build) | 1008/1008 identical (below) |

## CI

- `build-ps2`: the PS2 compiler in Docker (`tools/mwccps2/Dockerfile`) and a smoke test.
- `test-psp`, `test-ps2`: native build on Ubuntu with clang and lld, then `test.py` against the original under wibo
  (`tools/download_tool.py wibo`). `test-ps2` also checks that `native_ps2.py` regenerates `src/native/ps2/` as committed.
  Known failures are reported; anything else fails the job.
- `difftest-psp`: builds `origin/mwccpsp` and the commit, then `difftest.py --orig`.
- `mwccps2-release.yml`: a `mwccps2-*` tag builds Linux (Docker) and macOS arm64/x86-64 and attaches
  `mwccps2-native64-<os>-<arch>.tar.gz`.

The PS2 original comes from decomp.me's public image `ghcr.io/decompme/compilers/ps2/mwcps2-3.0.1b210-060308`
(`tools/mwccps2/get_orig.py`: anonymous registry token, the image's single layer by digest
`sha256:5ca2eaf1…`, `mwccps2.exe` SHA-256 `28654849…`, the same file the sotn-decomp work used).

What was run locally: `mwccps2-release.yml`'s macOS build (arm64 cross-compiled and x86-64, `native_build.py --ps2`)
and smoke test; the Linux jobs' steps on the Mac where they don't need Linux (`get_orig.py`, `download_tool.py wibo`,
`native_ps2.py` and the committed-tables check, `test.py`, `difftest.py`); the format and lint checks. Docker wasn't
running then, so the Docker builds were only checked as YAML; the refresh below ran them.

## The refresh to `988196355` (`1e2c7bc`)

`sync_sotn.py ../sotn-decomp --rev 988196355 --ps2`. Checked on an x86-64 Mac and, for the Linux jobs, in
`ubuntu:24.04` containers running `build.yml`'s steps:

| Check                                                        | Result |
| ------------------------------------------------------------ | ------ |
| `unifdef` of every file `919828d` has                        | 474/474 same tokens |
| mwccpsp `test.py` (macOS; Linux)                             | 342/360, the 18 known; the same on Linux |
| mwccps2 `test.py --ps2` (macOS; Linux)                       | 263/452, the 189 known; the same on Linux |
| `src/native/ps2` regenerated by `native_ps2.py`              | as committed (macOS and Linux) |
| `difftest.py --orig`, `origin/mwccpsp` → `1e2c7bc` (Linux)   | exit 0: 1,720 compiles, 1,200 fixed, 212 same, 226 same error, 28 both crash, 36 no longer crash but not the original's, 18 nondeterministic |
| `build-ps2` (`tools/mwccps2/Dockerfile`) and its smoke test  | builds; the object is MIPS ELF |
| PSP goldens                                                  | 2044/2044, 10198/10198 |
| P4 corpus, macOS build and the Docker Linux build            | 1008/1008 |

`g_data.c` was a corpus problem, not a compiler one: the unit uses `__FILE__`, and the corpus replays compiled every
source as `input.c`, so the string came out `"input.c"` (8 bytes, `.sdata`) where the captured object has
`"g_data.c"` (9 bytes, `.data`). Replayed under its own name (sotn-decomp `1d532dfe5`: `input_name` in the case's
`args.json`), the original, the hybrid and the native build all reproduce it.
