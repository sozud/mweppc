#!/usr/bin/env python3
"""Compile the same inputs with two builds of a compiler and require identical
results: the object file, the exit status and the messages.

    difftest.py --a build/old/mwccpsp --b build/new/mwccpsp [-j N] [--quick]

A regression check that needs no original compiler: --a is a build known to be
good (say, from the base branch), --b the one under test. The inputs are
tools/mwccpsp/tests (with test.py's flag sets) and, as a larger set, every C
file of this repository's own sources (src/), compiled as MWCC code with the
include paths config/sources.json gives it, at -O0, -Op, -O4,p and -O4,s.
Where --a crashes (a signal, or "not implemented": code without source yet)
--b may do anything, and is counted as "improved" when it doesn't crash;
everywhere else, errors included, the two must agree exactly.

With --orig (the original compiler, e.g. "wibo orig/PSP_3_0_1/mwccpsp.exe"),
a difference is judged against it: "fixed" when --b now agrees with the
original, a failure ("regression") only when --a agreed and --b doesn't, and
"changed" (reported, not failed) when neither agrees. Crashes of --a that --b
gets right count as "fixed" too. A case where --a gives a different result each
time is "nondeterministic" and not compared.
"""

import argparse
import concurrent.futures as cf
import glob
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import test  # noqa: E402

SRC_FLAGS = ["-O0", "-Op", "-O4,p", "-O4,s"]
# the sources' Win32 declarations, which a MIPS compiler doesn't take
PREFIX = "#define __stdcall\n#define __cdecl\n#define __declspec(x)\n"


def jobs(quick):
    out = []
    srcs = sorted(glob.glob(os.path.join(HERE, "tests", "*.c")) + glob.glob(os.path.join(HERE, "tests", "*.cpp")))
    for s in srcs:
        for f in test.FLAGS + test.PS2_FLAGS:
            out.append((s, [*test.COMMON, *test.LANG[os.path.splitext(s)[1]], *f.split()]))
    for s in sorted(glob.glob(os.path.join(HERE, "tests", "debug", "*.c*"))):
        for f in test.DEBUG_FLAGS:
            out.append((s, [*test.COMMON, *test.LANG[os.path.splitext(s)[1]], *f.split()]))
    for s in sorted(glob.glob(os.path.join(HERE, "tests", "diag", "*.c*"))):
        out.append((s, [*test.COMMON, *test.LANG[os.path.splitext(s)[1]], "-Op"]))
    table = json.load(open(os.path.join(REPO, "config", "sources.json")))
    units = sorted(glob.glob(os.path.join(REPO, "src", "**", "*.c"), recursive=True))
    for path in units:
        rel = os.path.relpath(path, REPO)
        if rel.startswith("src/native/"):
            continue  # host code
        inc = [os.path.dirname(rel), *table.get(rel, {}).get("include_dirs", [])]
        args = ["-c", "-lang", "c", "-I.", "-prefix", "prefix.h"]
        args += [f"-I{os.path.join(REPO, d)}" for d in dict.fromkeys(inc)]
        args += [f"-I{os.path.join(REPO, 'include')}", "-I-", f"-I{os.path.join(REPO, 'include', 'libc')}"]
        args += [f"-I{os.path.join(REPO, 'include')}", "-DVERSION=4"]
        for f in SRC_FLAGS[1:3] if quick else SRC_FLAGS:
            out.append((path, [*args, f]))
    return out


def compile_one(cc, src, args):
    work = tempfile.mkdtemp(prefix="mwcc-difftest-")
    try:
        open(os.path.join(work, "prefix.h"), "w").write(PREFIX)
        out = os.path.join(work, "out.o")
        try:
            p = subprocess.run([*cc, *args, "-o", out, src], cwd=work, capture_output=True, timeout=test.TIMEOUT)
            rc, text = p.returncode, test.normalize((p.stdout + p.stderr).decode("latin1"))
        except subprocess.TimeoutExpired:
            rc, text = "timeout", ""
        obj = open(out, "rb").read() if os.path.isfile(out) else None
        return rc, text, obj
    finally:
        shutil.rmtree(work, ignore_errors=True)


def crashed(r):
    rc, text, _ = r
    return rc == "timeout" or rc < 0 or "(native): not implemented" in text


def run(a, b, src, args, orig=None):
    ra, rb = compile_one(a, src, args), compile_one(b, src, args)
    if ra == rb:
        return ("same" if ra[2] is not None else "same-error"), None
    if compile_one(a, src, args) != ra:
        # the reference build differs from itself (uninitialised memory in
        # the output): nothing to compare
        return "nondeterministic", None
    if orig and not crashed(rb):
        ro = compile_one(orig, src, args)
        if rb == ro:
            return "fixed", None
        if ra == ro:
            return "regression", f"--- original: {ro[0]}\n{ro[1][-800:]}\n--- b: {rb[0]}\n{rb[1][-800:]}"
        return ("improved, not the original's" if crashed(ra) else "changed"), None
    if crashed(ra):
        return ("improved" if not crashed(rb) else "both-crash"), None
    what = [n for n, x, y in zip(("status", "messages", "object"), ra, rb) if x != y]
    detail = f"--- a: {ra[0]}\n{ra[1][-800:]}\n--- b: {rb[0]}\n{rb[1][-800:]}" if "object" not in what else ""
    return "differs (" + ", ".join(what) + ")", detail


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--a", required=True, help="the reference build")
    ap.add_argument("--b", required=True, help="the build under test")
    ap.add_argument("-j", type=int, default=4)
    ap.add_argument("--orig", help="the original compiler, to judge differences")
    ap.add_argument("-v", action="store_true", help="list the fixed and changed cases too")
    ap.add_argument("--quick", action="store_true", help="the sources at -Op and -O4,p only")
    a = ap.parse_args()
    ca = [os.path.abspath(x) if os.path.exists(x) else x for x in shlex.split(a.a)]
    cb = [os.path.abspath(x) if os.path.exists(x) else x for x in shlex.split(a.b)]
    co = [os.path.abspath(x) if os.path.exists(x) else x for x in shlex.split(a.orig)] if a.orig else None
    todo = jobs(a.quick)
    counts = {}
    bad = 0
    with cf.ThreadPoolExecutor(a.j) as ex:
        futs = {ex.submit(run, ca, cb, s, args, co): (s, args) for s, args in todo}
        for fut in cf.as_completed(futs):
            s, args = futs[fut]
            status, detail = fut.result()
            counts[status] = counts.get(status, 0) + 1
            if a.v and status in ("fixed", "changed", "improved, not the original's", "nondeterministic"):
                print(f"{status}: {os.path.relpath(s, REPO)} {' '.join(args[-1:])}")
            if status.startswith("differs") or status == "regression":
                bad += 1
                print(f"{status}: {os.path.relpath(s, REPO)} {args[-1]}")
                if detail:
                    print(detail)
    print(f"{len(todo)} compiles: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
