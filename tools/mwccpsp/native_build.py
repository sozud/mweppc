#!/usr/bin/env python3
"""Build the native mwccpsp: compile every unit, generate the glue for what
the sources leave undefined, and link.

    native_build.py [-j N] [--arch x86_64|arm64] [--asan] [--ps2] [-o OUT]

Needs clang (and lld on Linux), and the original orig/PSP_3_0_1/mwccpsp.exe for the
glue's data. --ps2 builds build/PS2_3_0_1/native/<arch>/mwccps2 from
orig/PS2_3_0_1/mwccps2.exe instead (see native.py).
"""

import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import native_glue  # noqa: E402

import native  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", type=int, default=os.cpu_count() or 2)
    ap.add_argument("--arch", default="arm64" if os.uname().machine in ("arm64", "aarch64") else "x86_64")
    ap.add_argument("--asan", action="store_true")
    ap.add_argument("-o", "--output")
    ap.add_argument("--ps2", action="store_true")
    ap.add_argument("--only", help="recompile only units whose path contains this (the rest are reused)")
    a = ap.parse_args()
    native.configure(a.ps2)
    py = sys.executable
    asan = ["--asan"] if a.asan else []
    if a.ps2:
        asan.append("--ps2")
    subprocess.run(
        [
            py,
            os.path.join(HERE, "native.py"),
            "-j",
            str(a.j),
            "--arch",
            a.arch,
            *asan,
            *(["--only", a.only] if a.only else []),
        ],
        check=True,
    )
    subprocess.run([py, os.path.join(HERE, "native_glue.py"), "--arch", a.arch, *asan], check=True)
    out = native.out_dir(a.arch, a.asan)
    glue = os.path.join(out, "glue")
    subprocess.run(
        [
            "clang",
            *native.arch_flags(a.arch),
            "-c",
            "-w",
            os.path.join(glue, "glue.c"),
            "-o",
            os.path.join(glue, "glue.o"),
        ],
        check=True,
    )
    objs = native.objects(a.arch, a.asan)
    missing = [o for o in objs if not os.path.exists(o)]
    if missing:
        sys.exit("units failed to compile: " + " ".join(os.path.basename(o) for o in missing))
    ldflags = open(os.path.join(glue, "ldflags.txt")).read().split()
    exe = os.path.join(out, native.name())
    subprocess.run(
        [
            "clang",
            *native.arch_flags(a.arch),
            *(native.SANITIZE[:1] if a.asan else []),
            "-o",
            exe,
            *objs,
            os.path.join(glue, "glue.o"),
            *ldflags,
            *native_glue.LINK,
            # packed tables hold pointers at 2-byte alignment, which arm64
            # chained fixups cannot relocate
            *(["-Wl,-no_fixup_chains"] if sys.platform == "darwin" and a.arch == "arm64" else []),
        ],
        check=True,
    )
    if a.output:
        shutil.copy2(exe, a.output)
    print(a.output or exe)


if __name__ == "__main__":
    main()
