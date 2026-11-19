#!/usr/bin/env python3
"""Download the original PS2 compiler 3.0.1 build 198 (mwccps2.exe, 2005-10-11)
into orig/PS2_3_0_1_b198/.

    get_orig.py [-o DIR]

decomp.me publishes its compilers as public container images
(github.com/decompme/compilers, ghcr.io/decompme/compilers/ps2/
mwcps2-3.0.1b198-051011). The image has a single layer, a tarball holding
compilers/ps2/mwcps2-3.0.1b198-051011/; this fetches that layer by its digest
(content-addressed, so the image tag moving doesn't change what is fetched),
checks it, and takes mwccps2.exe and the two license manager DLLs it loads.
"""

import argparse
import hashlib
import io
import json
import os
import tarfile
import urllib.request

REGISTRY = "https://ghcr.io"
IMAGE = "decompme/compilers/ps2/mwcps2-3.0.1b198-051011"
LAYER = "sha256:627867f8704e14e2c8d428520f6de2668d7712903b9571e9aecf1ce628b4c60b"
PREFIX = "compilers/ps2/mwcps2-3.0.1b198-051011/"
FILES = {
    "mwccps2.exe": "41983e892a7b4a34b3ba96c2dd9d45f6a52d96a5ac4120dc479246786b124184",
    "LMGR8C.DLL": "996041141858fd4724ec36add0913755a0067498b93271d7d26e4a1bfa04a561",
    "LMGR326B.DLL": "bd9415ab11f4aef42c372e8547a8303b02464b59549745be597cf09e460737e8",
}

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))


def get(url, headers=None):
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers or {}), timeout=60) as r:
        return r.read()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out", default=os.path.join(REPO, "orig", "PS2_3_0_1_b198"))
    a = ap.parse_args()
    if all(os.path.exists(os.path.join(a.out, f)) for f in FILES):
        have = {f: hashlib.sha256(open(os.path.join(a.out, f), "rb").read()).hexdigest() for f in FILES}
        if have == FILES:
            print(f"{a.out}: already there")
            return
    token = json.loads(get(f"{REGISTRY}/token?scope=repository:{IMAGE}:pull"))["token"]
    blob = get(f"{REGISTRY}/v2/{IMAGE}/blobs/{LAYER}", {"Authorization": f"Bearer {token}"})
    if "sha256:" + hashlib.sha256(blob).hexdigest() != LAYER:
        raise SystemExit(f"{IMAGE}: the layer doesn't match {LAYER}")
    os.makedirs(a.out, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(blob), mode="r:gz") as tar:
        for name, sha in FILES.items():
            data = tar.extractfile(PREFIX + name).read()
            if hashlib.sha256(data).hexdigest() != sha:
                raise SystemExit(f"{name}: SHA-256 mismatch")
            open(os.path.join(a.out, name), "wb").write(data)
    print(f"{a.out}: " + ", ".join(FILES))


if __name__ == "__main__":
    main()
