"""Minimal PE32 editing for mwccpsp.exe: read sections and imports, patch
bytes by virtual address, and append new sections.

New sections are placed after the last one; their contents may hold absolute
addresses, which is fine because wibo maps the image at its preferred base
(0x400000) and nothing in our sections is listed in .reloc.
"""

import struct

SCN_CODE = 0x00000020
SCN_IDATA = 0x00000040
SCN_EXEC = 0x20000000
SCN_READ = 0x40000000
SCN_WRITE = 0x80000000


def align(x, a):
    return (x + a - 1) // a * a


class PE:
    def __init__(self, path):
        self.data = bytearray(open(path, "rb").read())
        d = self.data
        self.pe = struct.unpack_from("<I", d, 0x3C)[0]
        self.opt = self.pe + 24
        self.optsize = struct.unpack_from("<H", d, self.pe + 20)[0]
        self.base = struct.unpack_from("<I", d, self.opt + 28)[0]
        self.sect_align, self.file_align = struct.unpack_from("<II", d, self.opt + 32)
        self.dirs = [struct.unpack_from("<II", d, self.opt + 96 + 8 * i) for i in range(16)]

    @property
    def nsec(self):
        return struct.unpack_from("<H", self.data, self.pe + 6)[0]

    @property
    def sections(self):
        out = []
        for i in range(self.nsec):
            s = self.opt + self.optsize + 40 * i
            name = bytes(self.data[s : s + 8]).rstrip(b"\0").decode()
            vsize, va, rsize, raw = struct.unpack_from("<IIII", self.data, s + 8)
            out.append((name, va, vsize, raw, rsize))
        return out

    def section(self, name):
        return next(s for s in self.sections if s[0] == name)

    def off(self, rva):
        for _, va, vs, raw, rs in self.sections:
            if va <= rva < va + max(vs, rs) and rva - va < rs:
                return rva - va + raw
        return None

    def read(self, va, n):
        o = self.off(va - self.base)
        return bytes(self.data[o : o + n])

    def write(self, va, b):
        o = self.off(va - self.base)
        assert o is not None, hex(va)
        self.data[o : o + len(b)] = b

    def relocations(self):
        """virtual addresses of every 32-bit base relocation (absolute
        pointers in the image)"""
        rva, size = self.dirs[5]
        out = set()
        o = self.off(rva)
        end = o + size
        while o < end:
            page, block = struct.unpack_from("<II", self.data, o)
            if block < 8:
                break
            for i in range((block - 8) // 2):
                e = struct.unpack_from("<H", self.data, o + 8 + 2 * i)[0]
                if e >> 12 == 3:  # IMAGE_REL_BASED_HIGHLOW
                    out.add(self.base + page + (e & 0xFFF))
            o += block
        return out

    def imports(self):
        """{(dll_lower, name): iat_va}"""
        d, out = self.data, {}
        rva, _ = self.dirs[1]
        p = self.off(rva)
        while True:
            ilt, _, _, nm, iat = struct.unpack_from("<IIIII", d, p)
            if nm == 0:
                break
            q = self.off(nm)
            dll = bytes(d[q : d.index(b"\0", q)]).decode().lower()
            t, i = self.off(ilt or iat), 0
            while True:
                v = struct.unpack_from("<I", d, t + 4 * i)[0]
                if v == 0:
                    break
                if not v & 0x80000000:
                    o = self.off(v) + 2
                    out[(dll, bytes(d[o : d.index(b"\0", o)]).decode())] = self.base + iat + 4 * i
                i += 1
            p += 20
        return out

    def next_section_va(self):
        last = max(self.sections, key=lambda s: s[1])
        return self.base + align(last[1] + last[2], self.sect_align)

    def add_section(self, name, data, flags):
        """Append a section; returns its VA. Call next_section_va() first if
        the contents need to know their own address."""
        d = self.data
        hdr = self.opt + self.optsize + 40 * self.nsec
        first_raw = min(s[3] for s in self.sections if s[3])
        assert hdr + 40 <= first_raw, "no room for another section header"
        va = self.next_section_va() - self.base
        raw = align(len(d), self.file_align)
        rsize = align(len(data), self.file_align)
        d.extend(b"\0" * (raw - len(d)))
        d.extend(data)
        d.extend(b"\0" * (rsize - len(data)))
        struct.pack_into("<8sIIIIIIHHI", d, hdr, name.encode()[:8], len(data), va, rsize, raw, 0, 0, 0, 0, flags)
        struct.pack_into("<H", d, self.pe + 6, self.nsec + 1)
        struct.pack_into("<I", d, self.opt + 56, align(va + len(data), self.sect_align))  # SizeOfImage
        return self.base + va

    def save(self, path):
        open(path, "wb").write(self.data)
