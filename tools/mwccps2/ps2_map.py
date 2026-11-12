"""Where the PS2 compiler (mwccps2.exe 3.0.1 build 210) keeps what the sources
name: the native build's glue and data generators look names up here.

config/PS2_3_0_1/symbols.txt gives each name's b210 address. The sources name
the PSP compiler's addresses (func_XXXXXXXX/data_XXXXXXXX are PSP addresses,
config/PSP_3_0_1/symbols.txt), and config/PS2_3_0_1/addrmap.csv moves those
to b210: exact correspondences read from functions that are identical in both
compilers, and between them the offset of the nearest mapped neighbours when
both agree. Both files come from the sotn-decomp tree, where the PS2 work is
done (tools/mwccpsp/sync_sotn.py --ps2).
"""

import bisect
import csv
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
CONFIG = os.path.join(REPO, "config", "PS2_3_0_1")

# MWCCPS2_BUILD picks the PS2 compiler build; only b210 has its data here
PS2_BUILD = int(os.environ.get("MWCCPS2_BUILD", "210"))


class AddrMap:
    def __init__(self, path):
        self.exact = {}
        for r in csv.DictReader(open(path)):
            self.exact[int(r["psp"], 16)] = int(r["ps2"], 16)
        self.keys = sorted(self.exact)

    def __call__(self, a, window=0x400):
        """PS2 address for PSP address `a`, or None."""
        if a in self.exact:
            return self.exact[a]
        i = bisect.bisect_left(self.keys, a)
        lo = self.keys[i - 1] if i > 0 else None
        hi = self.keys[i] if i < len(self.keys) else None
        dl = self.exact[lo] - lo if lo is not None and a - lo <= window else None
        dh = self.exact[hi] - hi if hi is not None and hi - a <= window else None
        if dl is not None and (dh is None or dl == dh):
            return a + dl
        if dh is not None and dl is None:
            return a + dh
        return None  # neighbours disagree: unknown


def ps2_address_map():
    """name -> b210 address, and the PSP -> b210 AddrMap."""
    addr = {}
    for line in open(os.path.join(CONFIG, "symbols.txt")):
        m = re.match(r"\s*(\S+)\s*=\s*0x([0-9A-Fa-f]+);", line)
        if m:
            addr[m.group(1)] = int(m.group(2), 16)
    return addr, AddrMap(os.path.join(CONFIG, "addrmap.csv"))
