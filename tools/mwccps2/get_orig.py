#!/usr/bin/env python3
"""Download the original PS2 compiler (mwccps2.exe 3.0.1 build 210, 2006-03-08,
the one Persona 4 was built with) into orig/PS2_3_0_1/.

    get_orig.py [-o DIR]

decomp.me publishes its compilers as public container images
(github.com/decompme/compilers, ghcr.io/decompme/compilers/ps2/
mwcps2-3.0.1b210-060308). The image has a single layer, a tarball holding
compilers/ps2/mwcps2-3.0.1b210-060308/; this fetches that layer by its digest
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
IMAGE = "decompme/compilers/ps2/mwcps2-3.0.1b210-060308"
LAYER = "sha256:5ca2eaf11ead2f925d804cecf1d8db3a1e9e70fa63a57cdbdd9b171f29c39554"
PREFIX = "compilers/ps2/mwcps2-3.0.1b210-060308/"
FILES = {
    "mwccps2.exe": "286548490e2e902cfef21dcf39cd5af23766731585d90dea747f8781eadcafd7",
    "LMGR8C.DLL": "996041141858fd4724ec36add0913755a0067498b93271d7d26e4a1bfa04a561",
    "LMGR326B.DLL": "bd9415ab11f4aef42c372e8547a8303b02464b59549745be597cf09e460737e8",
}

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))


def get(url, headers=None):
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers or {}), timeout=60) as r:
        return r.read()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out", default=os.path.join(REPO, "orig", "PS2_3_0_1"))
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
