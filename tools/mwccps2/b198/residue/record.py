#!/usr/bin/env python3
"""record.py CORPUS EXE OUTDIR CASES: per case, the original b198's residue words
(../../b119/residue/resid.py at 0x4D006F / 0x4D00A4) into OUTDIR/<case>.seq, and
whether the original's object equals expected.o in that run (OUTDIR/record.txt).
Run on Linux (ptrace) with WIBO pointing at wibo; the .seq files are replayed
into the native with MWCCPS2_B119_RESIDUE (replay with run_corpus.py)."""

import json
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
RESID = os.path.join(HERE, "..", "..", "b119", "residue", "resid.py")
if not os.path.exists(RESID):
    RESID = os.path.join(HERE, "resid.py")
corpus, exe, out, cases = (os.path.abspath(a) for a in sys.argv[1:5])
WIBO = os.environ.get("WIBO", "/opt/tools/wibo")
env = dict(os.environ, RESID_BP="0x4d006f", RESID_BP3="0x4d00a4")
os.makedirs(out, exist_ok=True)


def setup(c, work):
    if os.path.isdir(os.path.join(c, "root")):
        shutil.copytree(os.path.join(c, "root"), work, dirs_exist_ok=True)
    m = json.load(open(os.path.join(c, "args.json")))
    name = m.get("input_name", "input.c")
    os.makedirs(os.path.join(work, ".in"), exist_ok=True)
    os.makedirs(os.path.join(work, ".out"), exist_ok=True)
    shutil.copy(os.path.join(c, "input.c"), os.path.join(work, ".in", name))
    obj = os.path.join(work, ".out", "result.o")
    args = [
        x.replace("{input}", os.path.join(work, ".in", name)).replace("{output}", obj).replace("{cwd}", work)
        for x in m["argv"][1:]
    ]
    for i, x in enumerate(args):
        d = x[2:] if x.startswith("-I") else (args[i + 1] if x == "-i" and i + 1 < len(args) else None)
        if d and not os.path.isabs(d) and not d.startswith("{"):
            os.makedirs(os.path.join(work, d), exist_ok=True)
    return args, obj


def one(case):
    c = os.path.join(corpus, case)
    seq = os.path.join(out, case + ".seq")
    work = tempfile.mkdtemp(prefix="b198-case-")
    try:
        args, obj = setup(c, work)
        subprocess.run(
            [sys.executable, RESID, seq, "--", WIBO, exe, *args],
            cwd=work,
            env=env,
            capture_output=True,
            text=True,
            timeout=1800,
        )
        same = os.path.exists(obj) and open(obj, "rb").read() == open(os.path.join(c, "expected.o"), "rb").read()
        n = sum(1 for _ in open(seq)) if os.path.exists(seq) else -1
        return f"{case}\t{'orig-ok' if same else 'orig-DIFF'}\t{n}"
    finally:
        shutil.rmtree(work, ignore_errors=True)


ids = open(cases).read().split()
with ThreadPoolExecutor(int(os.environ.get("J", "4"))) as ex, open(os.path.join(out, "record.txt"), "a") as log:
    for line in ex.map(one, ids):
        log.write(line + "\n")
        log.flush()
        print(line, flush=True)
