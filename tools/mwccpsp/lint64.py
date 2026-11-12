#!/usr/bin/env python3
"""Inventory of 64-bit portability problems in tools/mwccpsp/src.

    lint64.py [--only CPrep] [-v] [--ps2]

Runs the host clang in 64-bit mode (-fsyntax-only) over every unit with the
same include paths as the CodeWarrior build, with warnings that flag code
assuming 32-bit pointers or 32-bit long. Prints counts per unit and per
warning; -v prints every diagnostic. This tracks Phase 4 of the plan: the
native build must be 64-bit clean before it can be trusted.
"""

import argparse
import collections
import concurrent.futures as cf
import json
import os
import re
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))

WARN = [
    "-Wpointer-to-int-cast",
    "-Wint-to-pointer-cast",
    "-Wshorten-64-to-32",
    "-Wvoid-pointer-to-int-cast",
    "-Wbad-function-cast",
]


PS2 = False  # --ps2: lint the PS2 build (-DMWCCPS2=1)


def lint(src, s):
    # flags follow mweppc's native (i386 Linux) build, retargeted to x86-64
    args = [
        "clang",
        "-fsyntax-only",
        "--target=x86_64-unknown-linux-gnu",
        "-fms-extensions",
        "-fno-ms-compatibility",
        "-D__INTEL__",
        "-D__option(x)=__option_##x",
        "-D__option_little_endian=1",
        "-D__option_longlong=1",
        "-xc",
        "-std=gnu17",
        "-fpack-struct=4",
        "-mms-bitfields",
        "-nostdlibinc",
        "-Ddllimport=",
        "-DMWCC_LINUX=1",
        "-fshort-enums",
        "-fgnu89-inline",
        "-fsigned-char",
        "-ferror-limit=0",
        "-Wno-everything",
        *WARN,
    ]
    args += ["-DVERSION=4", "-DMWCCPSP=1"] + (
        ["-DMWCCPS2=1", f"-DMWCCPS2_BUILD={int(os.environ.get('MWCCPS2_BUILD', '210'))}"] if PS2 else []
    )
    args += [f"-I{d}" for d in s.get("include_dirs", [])] + ["-Iinclude", "-Iinclude/libc"]
    p = subprocess.run([*args, src], cwd=TOP, capture_output=True, text=True)
    diags = re.findall(r"^(\S+):(\d+):\d+: (warning|error): (.*?)(?: \[(-W[\w-]+)\])?$", p.stderr, re.M)
    return src, diags


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("-v", action="store_true")
    ap.add_argument("--ps2", action="store_true")
    ap.add_argument("-j", type=int, default=4)
    a = ap.parse_args()
    global PS2
    PS2 = a.ps2
    table = json.load(open(os.path.join(TOP, "config", "sources.json")))
    units = [(k, v) for k, v in table.items() if k.endswith(".c") and os.path.isfile(os.path.join(TOP, k))]
    # the new 3.0.1 units (src/psp), as build.py adds them
    for root, _, files in os.walk(os.path.join(TOP, "src", "psp")):
        for f in sorted(files):
            rel = os.path.relpath(os.path.join(root, f), TOP)
            if f.endswith(".c") and rel not in table:
                units.append((rel, {"include_dirs": [os.path.dirname(rel)]}))
    if a.only:
        units = [u for u in units if a.only in u[0]]
    per_unit, per_flag, errors = {}, collections.Counter(), collections.Counter()
    with cf.ThreadPoolExecutor(a.j) as ex:
        for src, diags in ex.map(lambda u: lint(*u), units):
            w = [d for d in diags if d[2] == "warning"]
            e = [d for d in diags if d[2] == "error"]
            per_unit[src] = (len(w), len(e))
            for d in w:
                per_flag[d[4] or d[3][:40]] += 1
            errors[src] = len(e)
            if a.v:
                for f, line, kind, msg, flag in diags:
                    print(f"{f}:{line}: {kind}: {msg} {flag}")
    print("warnings by kind:")
    for k, v in per_flag.most_common():
        print(f"  {v:6}  {k}")
    print(f"units with errors (not yet compilable natively): {sum(1 for v in errors.values() if v)}/{len(units)}")
    worst = sorted(per_unit.items(), key=lambda kv: -kv[1][0])[:15]
    print("most warnings:")
    for k, (w, e) in worst:
        print(f"  {w:5} warnings {e:4} errors  {k}")


if __name__ == "__main__":
    main()
