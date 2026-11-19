#!/usr/bin/env python3
"""replay.py SEQDIR NATIVE OUT [-j N]: run each case that has SEQDIR/<case>.seq (record.py)
through the native with the original b198's residue words replayed (MWCCPS2_B119_RESIDUE).
OUT gets one line per case: case, pass/fail, residue words, and the hook's misalignment note."""

import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "../../../.."))
CORPUS = os.path.join(ROOT, "build/mwccps2-b198-corpus")
seqdir, native, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]), sys.argv[3]
j = int(sys.argv[sys.argv.index("-j") + 1]) if "-j" in sys.argv else 4


def one(case):
    seq = os.path.join(seqdir, case + ".seq")
    r = subprocess.run(
        [
            sys.executable,
            os.path.join(ROOT, "tools/mwccpsp/corpus/run_corpus.py"),
            "--corpus",
            CORPUS,
            "--cc",
            native,
            "-j",
            "1",
            "--cases",
            "/dev/stdin",
        ],
        input=case,
        text=True,
        capture_output=True,
        env={**os.environ, "MWCCPS2_B119_RESIDUE": seq},
        timeout=1800,
        cwd=ROOT,
    )
    ok = r.stdout.strip().splitlines()[-1].startswith("1/1") if r.stdout.strip() else False
    n = sum(1 for _ in open(seq))
    mis = open(seq + ".mis").read().strip() if os.path.exists(seq + ".mis") else ""
    if mis:
        os.unlink(seq + ".mis")
    return f"{case}\t{'pass' if ok else 'fail'}\t{n}\t{mis}"


cases = sorted(f[:-4] for f in os.listdir(seqdir) if f.endswith(".seq"))
with ThreadPoolExecutor(j) as ex, open(out, "w") as f:
    for line in ex.map(one, cases):
        f.write(line + "\n")
        f.flush()
