#!/usr/bin/env python3
"""Build the native mwccps2 3.0.1 build 198: build/PS2_3_0_1_b198/native/<arch>/mwccps2.

    native_b198.py [-j N] [--arch x86_64|arm64] [--asan] [--only UNIT] [-o OUT] [--gen]

The same sources as mwccps2 b210, compiled with -DMWCCPS2_BUILD=198 (where b198
differs from b210, the code is under MWCCPS2_BUILD guards). Needs the original
(get_orig.py) for the glue's data. --gen regenerates src/native/ps2/b198/ from it
first (tools/mwccps2/native_ps2.py), which the sources otherwise keep checked in.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))

if __name__ == "__main__":
    env = dict(os.environ, MWCCPS2_BUILD="198")
    args = sys.argv[1:]
    if "--gen" in args:
        args.remove("--gen")
        subprocess.run(
            [sys.executable, os.path.join(REPO, "tools", "mwccps2", "native_ps2.py"), "--gen-only"], env=env, check=True
        )
    sys.exit(
        subprocess.run(
            [sys.executable, os.path.join(REPO, "tools", "mwccpsp", "native_build.py"), "--ps2", *args], env=env
        ).returncode
    )
