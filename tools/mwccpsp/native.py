#!/usr/bin/env python3
"""Native 64-bit build of tools/mwccpsp/src.

    native.py [--only CPrep] [-j N] [-v] [--arch x86_64|arm64] [--ps2]

Compiles every unit with the host clang, as a
64-bit program against the host C library (which replaces MSL). Objects go to
build/PSP_3_0_1/native/<arch>/obj/. Prints how many units compile; -v prints the
errors.

Flags: -fwrapv -fno-strict-aliasing -fsigned-char (the
original assumes x86's signed char), and -fshort-enums because 3.0.1 was
built with minimal enums.

--ps2 builds the PS2 compiler (mwccps2.exe 3.0.1 b210) from the same sources
with -DMWCCPS2=1 into build/PS2_3_0_1/native/; its generated data units are
src/native/ps2/*.c (tools/mwccps2/native_ps2.py) in place of the
PSP ones.
"""

import argparse
import concurrent.futures as cf
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))
REPO = TOP
sys.path.insert(0, HERE)
from units import DRIVER_MACSPEC_UNITS, DRIVER_PLUGIN_UNITS, units  # noqa: E402

FLAGS = [
    "-std=gnu17",
    "-O1",
    "-g",
    "-fwrapv",
    "-fno-strict-aliasing",
    "-fsigned-char",
    "-fshort-enums",
    "-fms-extensions",
    "-fgnu89-inline",
    "-Wno-everything",
    "-ferror-limit=50",
    "-DVERSION=4",
    "-DMWCCPSP=1",
    "-DMWCCPSP_NATIVE=1",
]


# 3.0.1 indexes past some tables on purpose (RegClasses), and keeps pointers
# in pack(2) structs (the original layout; x86-64 and arm64 load them
# unaligned); ASan still checks memory
SANITIZE = [
    "-fsanitize=address,undefined",
    "-fno-sanitize=array-bounds,function,alignment",
    "-fno-omit-frame-pointer",
    "-fno-sanitize-recover=undefined",
]


# PS2 mode (--ps2): set by configure()
PS2 = False
# generated from the exe: PS2 mode takes these from src/native/ps2/
GENERATED = {"exe_data.c", "parser_tables.c", "strings.c"}
# the PS2 build traces which functions run (MWCCPS2_NATIVE_TRACE): every
# function gets an entry hook (src/native/ps2/trace.c)
# MWCCPS2_BUILD picks the PS2 compiler build (default 210, as build_ps2.py)
PS2_FLAGS = [
    "-DMWCCPS2=1",
    f"-DMWCCPS2_BUILD={int(os.environ.get('MWCCPS2_BUILD', '210'))}",
    "-finstrument-functions-after-inlining",
]


def configure(ps2):
    global PS2
    PS2 = ps2


def name():
    return "mwccps2" if PS2 else "mwccpsp"


def exe_path():
    return (
        os.path.join(REPO, "orig", "PS2_3_0_1", "mwccps2.exe")
        if PS2
        else os.path.join(REPO, "orig", "PSP_3_0_1", "mwccpsp.exe")
    )


def arch_flags(arch):
    # macOS clang takes -arch; elsewhere build for the host (or a target triple)
    if sys.platform == "darwin":
        return ["-arch", arch]
    host = {"x86_64": "x86_64", "amd64": "x86_64", "aarch64": "arm64", "arm64": "arm64"}.get(os.uname().machine)
    return [] if host == arch else [f"--target={'aarch64' if arch == 'arm64' else arch}-linux-gnu"]


def out_dir(arch, asan=False):
    return os.path.join(REPO, "build", "PS2_3_0_1" if PS2 else "PSP_3_0_1", "native", arch + ("-asan" if asan else ""))


def all_units(only=None):
    todo = list(units(only))
    # host-only units of the native build
    for f in sorted(os.listdir(os.path.join(TOP, "src", "native"))):
        rel = "src/native/" + f
        if f.endswith(".c") and (not only or only in rel) and not (PS2 and f in GENERATED):
            todo.append((rel, {}))
    if PS2:
        for f in sorted(os.listdir(os.path.join(TOP, "src", "native", "ps2"))):
            rel = "src/native/ps2/" + f
            if f.endswith(".c") and (not only or only in rel):
                todo.append((rel, {}))
    return todo


def obj_path(src, arch, asan=False):
    return os.path.join(out_dir(arch, asan), "obj", src.replace("/", "_") + ".o")


def objects(arch, asan=False):
    """the objects of the current units (the directory may hold stale ones)"""
    return [obj_path(src, arch, asan) for src, _ in all_units()]


def compile_unit(src, s, arch, asan=False):
    obj = obj_path(src, arch, asan)
    lang = ["-xc++", "-std=gnu++17"] if src.endswith(".cpp") else ["-xc"]
    args = ["clang", *arch_flags(arch), "-c", *FLAGS, *(SANITIZE if asan else []), *lang]
    if PS2:
        args += [f for f in PS2_FLAGS if not (src.startswith("src/native/") and f.startswith("-finstrument"))]
    if lang[0] == "-xc++":
        args = [a for a in args if a not in ("-std=gnu17", "-fgnu89-inline")]
    if src.startswith("src/driver/"):
        args.append("-DMWCCPSP_DRIVER=1")
        if os.path.basename(src) in DRIVER_PLUGIN_UNITS:
            args.append("-DMWCCPSP_DRIVER_PLUGIN=1")
        if os.path.basename(src) in DRIVER_MACSPEC_UNITS:
            args.append("-DMWCCPSP_MAC_FILESPEC=1")
    args += [f"-I{d}" for d in s.get("include_dirs", [])] + ["-Iinclude"]
    p = subprocess.run([*args, src, "-o", obj], cwd=TOP, capture_output=True, text=True)
    errs = re.findall(r"^(\S+):(\d+):\d+: error: (.*)$", p.stderr, re.M)
    return src, p.returncode == 0, errs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("-j", type=int, default=2)
    ap.add_argument("-v", action="store_true")
    ap.add_argument("--arch", default="x86_64")
    ap.add_argument("--asan", action="store_true", help="AddressSanitizer + UBSan build")
    ap.add_argument("--ps2", action="store_true", help="the PS2 compiler (mwccps2.exe b210)")
    a = ap.parse_args()
    configure(a.ps2)
    os.makedirs(os.path.join(out_dir(a.arch, a.asan), "obj"), exist_ok=True)
    todo = all_units(a.only)
    ok, failed = 0, []
    with cf.ThreadPoolExecutor(a.j) as ex:
        for src, good, errs in ex.map(lambda u: compile_unit(u[0], u[1], a.arch, a.asan), todo):
            if good:
                ok += 1
            else:
                failed.append((src, errs))
    first = {}
    for src, errs in failed:
        for f, line, msg in errs[:1]:
            key = re.sub(r"'[^']*'", "'X'", msg)[:70]
            first[key] = first.get(key, 0) + 1
        if a.v:
            for f, line, msg in errs:
                print(f"{f}:{line}: {msg}")
    print(f"{ok}/{len(todo)} units compile natively ({a.arch})")
    if first:
        print("first error of each failing unit, by kind:")
        for k, v in sorted(first.items(), key=lambda kv: -kv[1])[:25]:
            print(f"  {v:4}  {k}")


if __name__ == "__main__":
    main()
