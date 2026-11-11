#!/usr/bin/env python3
"""Compile the tests with the original compiler and another one and compare
the object files.

    test.py --cc build/PSP_3_0_1/native/x86_64/mwccpsp --orig "wibo orig/PSP_3_0_1/mwccpsp.exe"

Each file in tests/ is compiled with every flag set below by both compilers,
and the outputs must be byte-identical. The files in tests/diag/ must produce
diagnostics: there the exit status and the compiler's messages must be
identical instead. The files in tests/debug/ are compiled with debug info
(DEBUG_FLAGS).
"""

import argparse
import concurrent.futures as cf
import glob
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))

FLAGS = [
    "-Op -opt nointrinsics",
    "-O0 -opt nointrinsics",
    "-O4,p -opt nointrinsics",
    "-O4,s -opt nointrinsics",
    "-Op -opt intrinsics -fp single",
    "-O4,p -fp off",
    "-O4,p -fl fastmath",
]
DEBUG_FLAGS = ["-g -Op -opt nointrinsics", "-g -O0 -opt nointrinsics"]
COMMON = ["-c", "-sdatathreshold", "0", "-char", "unsigned"]
LANG = {".c": ["-lang", "c"], ".cpp": ["-lang", "c++", "-Cpp_exceptions", "on"]}


def compile_one(cc, src, flags, out):
    work = os.path.dirname(out)
    lang = LANG[os.path.splitext(src)[1]]
    proc = subprocess.run([*cc, *COMMON, *lang, *flags.split(), "-o", out, src], cwd=work, capture_output=True)
    if proc.returncode != 0 or not os.path.isfile(out):
        return None, (proc.stdout + proc.stderr).decode("latin1")[-1500:]
    return open(out, "rb").read(), None


def run(cc, orig, src, flags):
    work = tempfile.mkdtemp(prefix="mwccpsp-extra-")
    try:
        want, err = compile_one(orig, src, flags, os.path.join(work, "want.o"))
        if want is None:
            return "error(orig)", err
        got, err = compile_one(cc, src, flags, os.path.join(work, "got.o"))
        if got is None:
            return "error", err
        return ("ok" if got == want else "differs"), None
    finally:
        shutil.rmtree(work, ignore_errors=True)


def normalize(text):
    """What differs only by how each compiler was started: the program name in
    the banners, the drive and separators of file names (and so the width of
    the rule under them)."""
    text = text.replace("\r\n", "\n")
    text = re.sub(r"(?m)^### \S+ ", "### mwccpsp.exe ", text)
    text = re.sub(
        r"(?m)^(#\s+(?:File|In|From): )(.*)$",
        lambda m: m.group(1) + re.sub(r"^[A-Za-z]:", "", m.group(2)).replace("\\", "/"),
        text,
    )
    return re.sub(r"(?m)^# -+$", "# ---", text)


def run_diag(cc, orig, src):
    out = []
    for c in (orig, cc):
        work = tempfile.mkdtemp(prefix="mwccpsp-diag-")
        try:
            lang = LANG[os.path.splitext(src)[1]]
            p = subprocess.run([*c, *COMMON, *lang, "-Op", "-o", "out.o", src], cwd=work, capture_output=True)
            text = (p.stdout + p.stderr).decode("latin1")
            text = normalize(text)
            out.append((p.returncode, text))
        finally:
            shutil.rmtree(work, ignore_errors=True)
    if out[0] == out[1]:
        return "ok", None
    return "differs", "--- original\n%s\n--- ours\n%s" % (out[0][1][-1500:], out[1][1][-1500:])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", required=True, help="compiler command under test")
    ap.add_argument("--orig", required=True, help="original compiler command")
    ap.add_argument("-j", type=int, default=4)
    a = ap.parse_args()
    # the compilers run in temporary directories: make relative paths absolute
    cc = [os.path.abspath(x) if os.path.exists(x) else x for x in shlex.split(a.cc)]
    orig = [os.path.abspath(x) if os.path.exists(x) else x for x in shlex.split(a.orig)]
    srcs = sorted(glob.glob(os.path.join(HERE, "tests", "*.c")) + glob.glob(os.path.join(HERE, "tests", "*.cpp")))
    diags = sorted(
        glob.glob(os.path.join(HERE, "tests", "diag", "*.c")) + glob.glob(os.path.join(HERE, "tests", "diag", "*.cpp"))
    )
    debug = sorted(
        glob.glob(os.path.join(HERE, "tests", "debug", "*.c"))
        + glob.glob(os.path.join(HERE, "tests", "debug", "*.cpp"))
    )
    jobs = (
        [(s, f) for s in srcs for f in FLAGS]
        + [(s, f) for s in debug for f in DEBUG_FLAGS]
        + [(s, "diag") for s in diags]
    )
    bad = 0
    with cf.ThreadPoolExecutor(a.j) as ex:
        futs = {
            (ex.submit(run_diag, cc, orig, s) if f == "diag" else ex.submit(run, cc, orig, s, f)): (s, f)
            for s, f in jobs
        }
        for fut in cf.as_completed(futs):
            s, f = futs[fut]
            status, msg = fut.result()
            if status != "ok":
                bad += 1
                print(f"{status:12} {os.path.basename(s)} {f}")
                if msg:
                    print(msg)
    print(f"{len(jobs) - bad}/{len(jobs)} identical")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
